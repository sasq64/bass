#include "cart.h"
#include "defines.h"
#include "emulator.h"
#include "machine.h"

#include <coreutils/algorithm.h>
#include <coreutils/file.h>
#include <coreutils/log.h>
#include <coreutils/text.h>

#include <fmt/format.h>

#include <lib.h>
#include <shrink_inmem.h>

#include <algorithm>
#include <array>
#include <map>
#include <vector>

using namespace std::string_literals;

struct EmuPolicy : public sixfive::DefaultPolicy
{
    explicit EmuPolicy(sixfive::Machine<EmuPolicy>& m) : machine(&m) {}

    static constexpr bool UseSwitch = false;

    static constexpr bool ExitOnStackWrap = true;

    // PC accesses does not normally need to work in IO areas
    static constexpr int PC_AccessMode = sixfive::Callback;

    // Generic reads and writes should normally not be direct
    static constexpr int Read_AccessMode = sixfive::Callback;
    static constexpr int Write_AccessMode = sixfive::Callback;

    static constexpr int MemSize = 65536;

    std::array<Intercept*, 64 * 1024> intercepts{};

    std::unordered_map<uint32_t, std::function<void(uint32_t)>> jsrFunctions;

    sixfive::Machine<EmuPolicy>* machine;

    int breakId = -1;

    bool doTrace = false;

    // This function is run after each opcode. Return true to stop emulation.
    static bool eachOp(EmuPolicy& policy)
    {
        static unsigned last_pc = 0xffffffff;
        auto pc = policy.machine->regPC();

        if (pc != last_pc) {
            //auto code = policy.machine->read_ram(pc);
            if (policy.doTrace) {
                uint32_t pp = pc;
                fmt::print("{:04x} {}\n", pc, Machine::disassemble(*policy.machine, &pp));
            }
            if (auto* ptr = policy.intercepts[pc]) {
                return ptr->fn(pc);
            }
            last_pc = pc;
        }
        return false;
    }

    static bool breakFn(EmuPolicy& policy, int n)
    {
        policy.breakId = n;
        fmt::print("BRK in {:04x}\n", policy.machine->regPC());
        return true;
    }
    static constexpr uint32_t jsrMask = 0xffffffff;

    static bool jsrFn(EmuPolicy& p, uint32_t target) {
        //fmt::print("JSR {:04x}\n", target);
        auto it = p.jsrFunctions.find(target);
        if (it != p.jsrFunctions.end()) {
            it->second(target);
            return true;
        }
        return false;
    }
};

void Machine::SetTracing(bool b) {
    machine->policy().doTrace = b;

}

void Machine::addJsrFunction(uint32_t address,
                             const std::function<void(uint32_t)>& fn)
{
    fmt::print("JSR {:x}\n", address);
    machine->policy().jsrFunctions[address] = fn;
}

void Machine::addIntercept(uint32_t address,
                           std::function<bool(uint32_t)> const& fn)
{
    auto*& ptr = machine->policy().intercepts[address & 0xffff];
    if (ptr == nullptr) {
        ptr = new Intercept();
    }
    ptr->type = Call;
    ptr->fn = fn;
}

inline void Check(bool v, std::string const& txt)
{
    if (!v) throw machine_error(txt);
}

Machine::Machine(uint32_t start)
{
    machine = std::make_unique<sixfive::Machine<EmuPolicy>>();
    addSection({"default", start});
    setSection("default");
}

void Machine::setCpu(CPU cpu)
{
    cpu65C02 = cpu == CPU_65C02;
    machine->set_cpu(cpu65C02);
}

Machine::~Machine() = default;

Section& Machine::addSection(Section const& s)
{
    auto [name, in, children, start, pc, size, flags, data, valid] = s;

    if (name.empty()) {
        name = "__anon_" + std::to_string(anonSection++);
    }

    auto it =
        std::find_if(sections.begin(), sections.end(),
                     [n=name](auto const& as) { return as.name == n; });

    Section& section =
        it == sections.end() ? sections.emplace_back(name, -1) : *it;

    Check(section.data.empty(),
          fmt::format("Section {} already populated", section.name));

    section.flags = flags;
    section.pc = pc;
    if (size != -1) {
        section.size = size;
        section.flags |= SectionFlags::FixedSize;
    }

    if (start != -1) {
        section.start = start;
        section.flags |= SectionFlags::FixedStart;
    }

    if (!in.empty()) {
        auto& parent = getSection(in);
        LOGD("Parent %s at %x/%x", parent.name, parent.start, parent.pc);
        Check(parent.data.empty(), "Parent section must contain no data");

        if (section.parent.empty()) {
            section.parent = in;
            parent.children.push_back(section.name);
        }

        if ((parent.flags & SectionFlags::ReadOnly) != 0) {
            section.flags |= SectionFlags::ReadOnly;
        }

        if (section.start == -1) {
            LOGD("Setting start to %x", parent.pc);
            section.start = parent.pc;
        }
    }

    if (section.pc == -1) {
        section.pc = section.start;
    }

    Check(section.start != -1, "Section must have start");

    return section;
}

void Machine::mapFile(std::string const& name, uint8_t bank)
{
    const utils::File f{name};
    //uint16_t start = f.read<uint8_t>();
    //start |= (f.read<uint8_t>() << 8);
    std::vector<uint8_t> data(f.getSize());
    f.read(data.data(), data.size());

    auto len = (std::ssize(data)+255)/256;
    setBankRead(bank, (int)len, [d=std::move(data), bank](uint32_t adr) {
        return d[adr - (bank<<8)];
    });

}

void Machine::removeSection(std::string const& name)
{
    auto it = std::find_if(sections.begin(), sections.end(),
                           [&](auto const& s) { return s.name == name; });
    if (it != sections.end()) {
        sections.erase(it);
    }
}

// Layout section 's', exactly at address if Floating, otherwise
// it must at least be placed after address
// Return section end
int32_t Machine::layoutSection(int32_t start, Section& s)
{
    if (!s.valid) {
        LOGI("Skipping invalid section %s", s.name);
        return start;
    }

    LOGD("Layout %s", s.name);
    if ((s.flags & FixedStart) == 0) {
        if (s.start != start) {
            LOGD("%s: %x differs from %x", s.name, s.start, start);
            layoutOk = false;
        }
        s.start = start;
    }

    Check(s.start >= start,
          fmt::format("Section {} starts at {:x} which is before {:x}", s.name,
                      s.start, start));

    if (!s.data.empty()) {
        Check(s.children.empty(), "Data section may not have children");
        // Leaf / data section
        return s.start + static_cast<int32_t>(s.data.size());
    }

    if (!s.children.empty()) {
        // Lay out children
        for (auto const& child : s.children) {
            start = layoutSection(start, getSection(child));
        }
    }
    // Unless fixed size, update size to total of its children
    if ((s.flags & FixedSize) == 0) {
        s.size = start - s.start;
    }
    if (start - s.start > s.size) {
        throw machine_error(fmt::format("Section {} is too large", s.name));
    }
    return s.start + s.size;
}

bool Machine::layoutSections()
{
    layoutOk = true;
    // Lay out all root sections
    for (auto& s : sections) {
        if (s.parent.empty()) {
            // LOGI("Root %s at %x", s.name, s.start);
            auto start = s.start;
            layoutSection(start, s);
        }
    }
    return layoutOk;
}

Error Machine::checkOverlap()
{
    for (auto const& a : sections) {
        if (!a.data.empty() && ((a.flags & NoStorage) == 0)) {
            for (auto const& b : sections) {
                if (&a != &b && !b.data.empty() &&
                    ((b.flags & NoStorage) == 0)) {
                    auto as = a.start;
                    auto ae = as + static_cast<int32_t>(a.data.size());
                    auto bs = b.start;
                    auto be = bs + static_cast<int32_t>(b.data.size());
                    if (as >= bs && as < be) {
                        return {2, 0,
                                fmt::format("Section {} overlaps {}", a.name,
                                            b.name)};
                    }
                    if (bs >= as && bs < ae) {
                        return {2, 0,
                                fmt::format("Section {} overlaps {}", b.name,
                                            a.name)};
                    }
                }
            }
        }
    }
    return {};
}

Section& Machine::getSection(std::string const& name)
{
    auto it = std::find_if(sections.begin(), sections.end(),
                           [&](auto const& s) { return s.name == name; });
    if (it == sections.end()) {
        throw machine_error(fmt::format("Unknown section {}", name));
    }
    return *it;
}
Section& Machine::getCurrentSection()
{
    return *currentSection;
}
void Machine::pushSection(std::string const& name)
{
    savedSections.push_back(currentSection);
    setSection(name);
}

void Machine::dropSection()
{
    savedSections.pop_back();
}

void Machine::popSection()
{
    currentSection = savedSections.back();
    savedSections.pop_back();
}

void Machine::setSection(std::string const& name)
{
    currentSection = &getSection(name);
    currentSection->valid = true;
}

void Machine::clear()
{
    anonSection = 0;
    dis.clear();
    for (auto& s : sections) {
        s.data.clear();
        s.pc = s.start;
        s.valid = false;
    }
    setSection("default");
}

uint32_t Machine::getPC() const
{
    if (currentSection == nullptr) {
        return 0;
    }
    return currentSection->pc;
}

// clang-format off
constexpr static std::array modeTemplate = {
        "",
        "A",

        "#${:02x}",
        "${:04x}",

        "${:02x}",
        "${:02x},x",
        "${:02x},y",
        "(${:02x},x)",
        "(${:02x}),y",
        "(${:02x})",

        "(${:04x})",
        "${:04x}",
        "${:04x},x",
        "${:04x},y",
        "${:x}"
};
// clang-format on

void writeChip(utils::File const& outFile, int bank, int startAddress,
               std::vector<uint8_t> const& data)
{
    outFile.writeString("CHIP");
    outFile.writeBE<uint32_t>(data.size() + 0x10);
    outFile.writeBE<uint16_t>(ChipType::Rom);
    outFile.writeBE<uint16_t>(bank);
    outFile.writeBE<uint16_t>(startAddress);
    outFile.writeBE<uint16_t>(data.size());
    outFile.write(data);
}

struct Chip
{
    int bank = 0;
    uint16_t start = 0;
    std::vector<uint8_t> data = std::vector<uint8_t>(0x2000);
};

void Machine::writeCrt(utils::File const& outFile)
{
    std::map<uint32_t, Chip> chips;
    bool banked = false;
    uint8_t exrom = 0;
    uint8_t game = 1;

    // 8000 -> bfff
    // e000 -> ffff
    for (auto const& section : sections) {
        if (section.data.empty()) {
            continue;
        }
        LOGI("Start %x", section.start);
        auto bank = section.start >> 16;
        auto start = section.start & 0xffff;
        auto end = start + section.data.size();
        // bank : 01a000,01c000,01e000
        if (start < 0x8000 || end > 0xc000) {
            throw machine_error("Can't write crt");
        }

        if (end >= 0xa000) {
            game = 0;
        }

        auto offset = start - 0x8000;

        if (bank > 0) {
            banked = true;
        }

        auto& chip = chips[bank << 16 | 0x8000];
        LOGI("Putting section %s in %x at offset %x", section.name, bank,
             offset);
        if (static_cast<int32_t>(chip.data.size()) <= offset) {
            chip.data.resize(0x4000);
        }
        memcpy(&chip.data[offset], section.data.data(), section.data.size());
    }

    const uint32_t headerLength = 0x40;
    const uint16_t version = 0x0100;
    const uint16_t hardware =
        banked ? CartType::EasyFlash : CartType::Normalcartridge;

    std::string name = "TEST";
    std::array<char, 32> label{};
    std::copy(name.begin(), name.end(), label.data());

    outFile.writeString("C64 CARTRIDGE   ");
    outFile.writeBE(headerLength);
    outFile.writeBE(version);
    outFile.writeBE(hardware);
    outFile.write<uint8_t>(exrom);
    outFile.write<uint8_t>(game);
    outFile.write(std::vector<uint8_t>{0, 0, 0, 0, 0, 0});
    outFile.write(label);

    for (auto const& e : chips) {
        auto bank = e.first >> 16;
        auto start = e.first & 0xffff;
        LOGD("Writing %x/%x", bank, start);
        writeChip(outFile, bank, start, e.second.data);
    }
}

void Machine::writeListFile(std::string_view name)
{
    utils::File f{name, utils::File::Mode::Write};
    for (auto const& [pc, text] : dis) {
        f.writeString(fmt::format("{:04x} : {}\n", pc, text));
    }
}

void Machine::write(std::string_view name, OutFmt fmt)
{
    auto non_empty = utils::filter_to(sections, [](auto const& s) {
        return !s.data.empty() && ((s.flags & NoStorage) == 0);
    });

    if (non_empty.empty()) {
        puts("**Warning: No sections");
        return;
    }

    LOGD("%d data sections", non_empty.size());

    std::sort(non_empty.begin(), non_empty.end(),
              [](auto const& a, auto const& b) { return a.start < b.start; });

    int32_t last_end = -1;

    utils::File outFile = createFile(name);

    auto start = non_empty.front().start;
    auto end = non_empty.back().start +
               static_cast<int32_t>(non_empty.back().data.size());

    if (end <= start) {
        puts("**Warning: No code generated");
        return;
    }

    if (fmt == OutFmt::Crt) {
        writeCrt(outFile);
        return;
    }

    if (fmt == OutFmt::Prg) {
        outFile.write<uint8_t>(start & 0xff);
        outFile.write<uint8_t>(start >> 8);
    }

    if (fmt == OutFmt::PackedPrg) {
        auto [low, high] = runSetup();
        std::vector<uint8_t> target(high - low);
        machine->read_ram(low, target.data(), target.size());

        std::vector<uint8_t> packed(0x10000);
        int const flags = LZSA_FLAG_RAW_BACKWARD | LZSA_FLAG_RAW_BLOCK;
        auto packed_size =
            lzsa_compress_inmem(target.data(), packed.data(), target.size(),
                                packed.size(), flags, 0, 1);
        packed.resize(packed_size);
        outFile.write(packed);
        return;
    }

    // bool bankFile = false;

    for (auto const& section : non_empty) {
        if (section.data.empty()) {
            continue;
        }
        if ((section.flags & WriteToDisk) != 0) {
            auto of = createFile(section.name);
            of.write<uint8_t>(section.start & 0xff);
            of.write<uint8_t>(section.start >> 8);
            of.write(section.data);
            of.close();
            continue;
        }

        if ((section.flags & NoStorage) != 0) {
            continue;
        }

        // fmt::print("{} {:04x}->{:04x}\n", section.name, section.start,
        //            section.data.size() + section.start);

        if (section.start < last_end) {
            throw machine_error(
                fmt::format("Section {} overlaps previous", section.name));
        }

        auto offset = section.start;

        if (last_end >= 0) {
            while (last_end < offset) {
                outFile.write<uint8_t>(0);
                last_end++;
            }
        }

        last_end = offset + std::ssize(section.data);

        outFile.write(section.data);
    }
}

uint32_t Machine::run(uint16_t pc)
{
    runSetup();
    //fmt::print("Running code at ${:x}\n", pc);
    return go(pc);
}

uint32_t Machine::go(uint16_t pc)
{
    machine->setPC(pc);
    return machine->run();
}

void Machine::setPC(uint32_t pc)
{
    machine->setPC(pc);
}

void Machine::doIrq(uint16_t adr)
{
    machine->irq(adr);
}

bool Machine::runCycles(uint32_t cycles)
{
    auto realCycles = machine->run(cycles);
    if (realCycles == 0) { return true; }
    if (machine->policy().breakId >= 0) {
        return true;
    }
    return false;
}

std::pair<uint32_t, uint32_t> Machine::runSetup()
{
    uint32_t low = 0;
    uint32_t high = 0;
    machine->clear_ram();
    for (auto const& section : sections) {
        if (section.start < 0x10000 && !section.data.empty()) {
            if ((section.flags & NoStorage) != 0) {
                continue;
            }
            if (low == 0) {
                low = section.start;
            }
            high = section.start + section.data.size();
            //LOGI("Writing '%s' to %x-%x", section.name, section.start,
            //     section.start + section.data.size());
            machine->write_ram(section.start, section.data.data(),
                               section.data.size());
        }
    }

    return {low, high};
}

uint32_t Machine::writeByte(uint8_t b)
{
    dis[currentSection->pc] = fmt::format("{:02x}", b);
    currentSection->data.push_back(b);
    currentSection->pc++;
    return currentSection->pc;
}

uint32_t Machine::writeChar(uint8_t b)
{
    dis[currentSection->pc] = fmt::format("{:02x}", b);
    currentSection->data.push_back(b);
    currentSection->pc++;
    return currentSection->pc;
}

std::string Machine::disassemble(sixfive::Machine<EmuPolicy>& m, uint32_t* pc)
{
    auto code = m.read_mem(*pc);
    (*pc)++;
    auto const& instructions = sixfive::Machine<EmuPolicy>::getInstructions();
    sixfive::Machine<EmuPolicy>::Opcode opcode{};

    std::string name = "???";
    for (auto const& i : instructions) {
        for (auto const& o : i.opcodes) {
            if (o.code == code) {
                name = i.name;
                opcode = o;
                break;
            }
        }
    }
    auto sz = opSize(opcode.mode);
    int32_t arg = 0;
    if (sz == 3) {
        arg = m.read_mem(*pc) | (m.read_mem((*pc)+1) << 8);
        *pc += 2;
    } else if (sz == 2) {
        arg = m.read_mem((*pc)++);
    }

    if (opcode.mode == sixfive::Mode::REL) {
        arg = (static_cast<int8_t>(arg)) + *pc;
    }

    auto argstr = fmt::format(modeTemplate.at(static_cast<int>(opcode.mode)), arg);
    return fmt::format("{} {}", name, argstr);
}

// Check if ourMode can be converted to targetMode, given that the opcode argument
// is either 8 bit (small = true) or 16 bit (small = false).
bool Machine::modeMatches(sixfive::Mode ourMode, sixfive::Mode targetMode, bool small)
{
    using sixfive::Mode;
    if (targetMode == ourMode) { return true; }
    if((ourMode == Mode::ABS || ourMode == Mode::ADR) &&
            targetMode == Mode::REL) {
        return true;
    }
    if (small) {
        if (ourMode == Mode::IND && targetMode == Mode::INDZ) {
            return true;
        }
        if (ourMode == Mode::ADR && targetMode == Mode::ZP) {
            return true;
        }
        if (ourMode == Mode::ADRX && targetMode == Mode::ZPX) {
            return true;
        }
        if (ourMode == Mode::ADRY && targetMode == Mode::ZPY) {
            return true;
        }
    }
    if (ourMode == Mode::ADR && targetMode == Mode::ABS) {
        return true;
    }
    if (ourMode == Mode::ADRX && targetMode == Mode::ABSX) {
        return true;
    }
    if (ourMode == Mode::ADRY && targetMode == Mode::ABSY) {
        return true;
    }
    return false;
}

AsmResult Machine::assemble(Instruction const& instr)
{
    using sixfive::Mode;

    auto arg = instr;
    arg.opcode = utils::toLower(std::string(instr.opcode));

    auto const& instructions =
        sixfive::Machine<EmuPolicy>::getInstructions(cpu65C02);

    auto small = (arg.val >= 0 && arg.val <= 0xff);

    if (arg.mode == Mode::ZP_REL) {
        // Handle 65c02 bbrX & rmbX opcodes
        auto bit = arg.val >> 24;
        arg.val &= 0xffffff;
        arg.opcode = arg.opcode + std::to_string(bit);
    }

    // Find a matching opcode
    auto it_ins = utils::find_if(
        instructions, [&](auto const& i) { return i.name == arg.opcode; });

    if (it_ins == instructions.end()) {
        return AsmResult::NoSuchOpcode;
    }

    // Find a matching addressing mode
    auto it_op =
        std::find_if(it_ins->opcodes.begin(), it_ins->opcodes.end(),
                     [&](auto const& o) { 
                        return modeMatches(arg.mode, o.mode, small);
                     });

    if (it_op == it_ins->opcodes.end()) {
        return AsmResult::IllegalAdressingMode;
    }
    arg.mode = it_op->mode;

    if (arg.mode == Mode::REL) {
        arg.val = arg.val - currentSection->pc - 2;
    }

    if (arg.mode == Mode::ZP_REL) {
        auto adr = arg.val & 0xffff;
        auto val = (arg.val >> 16) & 0xff;
        auto diff = adr - currentSection->pc - 2;
        arg.val = diff << 8 | val;
    }

    auto sz = opSize(arg.mode);

    auto& cs = *currentSection;
    auto v = arg.val & (sz == 2 ? 0xff : 0xffff);
    if (arg.mode == sixfive::Mode::REL) {
        v = (static_cast<int8_t>(v)) + 2 + cs.pc;
    }

    dis[cs.pc] = fmt::format(
        "{} "s + modeTemplate.at(static_cast<int>(arg.mode)), it_ins->name, v);

    cs.data.push_back(it_op->code);
    if (sz > 1) {
        cs.data.push_back(arg.val & 0xff);
        dis.erase(cs.pc + 1);
    }
    if (sz > 2) {
        cs.data.push_back(arg.val >> 8);
        dis.erase(cs.pc + 2);
    }
    cs.pc += sz;

    if (arg.mode == Mode::REL && (arg.val > 127 || arg.val < -128)) {
        return AsmResult::Truncated;
    }

    return AsmResult::Ok;
}

uint8_t Machine::readMem(uint16_t adr) const
{
    return machine->read_mem(adr);
}

uint8_t Machine::readRam(uint16_t offset) const
{
    return machine->read_ram(offset);
}
void Machine::writeRam(uint16_t offset, uint8_t val)
{
    machine->write_ram(offset, val);
}

void Machine::writeRam(uint16_t org, const uint8_t* data, size_t size)
{
    machine->write_ram(org, data, size);
}

void Machine::bankWriteFunction(uint16_t adr, uint8_t val, void* data)
{
    auto* thiz = static_cast<Machine*>(data);
    thiz->bank_write_functions[adr >> 8](adr, val);
}

uint8_t Machine::bankReadFunction(uint16_t adr, void* data)
{
    auto* thiz = static_cast<Machine*>(data);
    return thiz->bank_read_functions[adr >> 8](adr);
}

void Machine::setBankWrite(int hi_adr, int len,
                           std::function<void(uint16_t, uint8_t)> const& fn)
{
    for (int i=0; i<len; i++) {
        bank_write_functions[hi_adr+i] = fn;
    }
    machine->map_write_callback(hi_adr, len, this, bankWriteFunction);
}

void Machine::setBankRead(int hi_adr, int len,
                          std::function<uint8_t(uint16_t)> const& fn)
{
    for (int i=0; i<len; i++) {
        bank_read_functions[hi_adr+i] = fn;
    }
    machine->map_read_callback(hi_adr, len, this, bankReadFunction);
}

void Machine::setBankRead(int hi_adr, int len, int bank)
{
    Section const* bankSection = nullptr;
    int32_t const adr = (bank << 16) | (hi_adr << 8);
    for (auto const& section : sections) {
        if (section.start == adr) {
            bankSection = &section;
            break;
        }
    }
    Check(bankSection != nullptr, "Could not map bank");
    machine->map_rom(hi_adr, bankSection->data.data(), len);
}

std::vector<uint8_t> Machine::getRam()
{
    std::vector<uint8_t> data(0x10000);
    machine->read_ram(0, data.data(), data.size());
    return data;
}

void Machine::setRegs(RegState const& regs)
{
    using sixfive::Reg;
    auto const& r = regs.regs;
    machine->set<Reg::A>(r[0]);
    machine->set<Reg::X>(r[1]);
    machine->set<Reg::Y>(r[2]);
    // machine->set<Reg::SR>(r[3]);
    // machine->set<Reg::SP>(r[4]);
    // machine->set<Reg::PC>(r[5]);
    for (auto const& [adr, val] : regs.ram) {
        machine->write_ram(adr, val);
    }
}

void Machine::setReg(sixfive::Reg reg, unsigned v)
{
    using sixfive::Reg;
    switch (reg) {
    case Reg::A:
        return machine->set<Reg::A>(v);
    case Reg::X:
        return machine->set<Reg::X>(v);
    case Reg::Y:
        return machine->set<Reg::Y>(v);
    case Reg::SR:
        return machine->set<Reg::SR>(v);
    case Reg::SP:
        return machine->set<Reg::SP>(v);
    case Reg::PC:
        return machine->set<Reg::PC>(v);
    }
}

unsigned Machine::getReg(sixfive::Reg reg)
{
    using sixfive::Reg;
    switch (reg) {
    case Reg::A:
        return machine->get<Reg::A>();
    case Reg::X:
        return machine->get<Reg::X>();
    case Reg::Y:
        return machine->get<Reg::Y>();
    case Reg::SR:
        return machine->get<Reg::SR>();
    case Reg::SP:
        return machine->get<Reg::SP>();
    case Reg::PC:
        return machine->get<Reg::PC>();
    }
    return 0; // Cant get here
}
