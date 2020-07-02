#include "defines.h"
#include "emulator.h"

#include "machine.h"

#include <coreutils/file.h>
#include <coreutils/log.h>

#include <algorithm>
#include <array>
#include <vector>

Machine::Machine()
{
    machine = std::make_unique<sixfive::Machine<>>();
    addSection("main", 0);
    machine->setBreakFunction(breakFunction, this);
}

void Machine::breakFunction(int what, void* data)
{
    auto* thiz = static_cast<Machine*>(data);

    auto it = thiz->break_functions.find(what);
    if (it != thiz->break_functions.end()) {
        it->second(what);
    } else {
        auto [a, x, y, sr, sp, pc] = thiz->getRegs();
        fmt::print("A:{:x} X:{:x} Y:{:x} {:x}\n", a, x, y, what, thiz->getPC());
    }
}

Machine::~Machine() = default;

void Machine::setBreakFunction(uint8_t what,
                               std::function<void(uint8_t)> const& fn)
{
    break_functions[what] = fn;
}

Section& Machine::addSection(std::string const& name, uint32_t start)
{
    if (!name.empty()) {
        if (fp != nullptr) {
            // printf(fp, "SECTION %s\n", name.c_str());
        }
        auto it = std::find_if(sections.begin(), sections.end(),
                               [&](auto const& s) { return s.name == name; });
        if (it == sections.end()) {
            currentSection = &sections.emplace_back(name, start);
        } else {
            currentSection = &(*it);
            if (!currentSection->data.empty()) {
                throw machine_error(
                    fmt::format("Section {} already exists", name));
            }
            currentSection->start = start;
        }
    } else {
        currentSection = &sections.emplace_back("", start);
    }
    return *currentSection;
}

Section& Machine::setSection(std::string const& name)
{
    if (!name.empty()) {
        auto it = std::find_if(sections.begin(), sections.end(),
                               [&](auto const& s) { return s.name == name; });
        if (it != sections.end()) {
            currentSection = &(*it);
            return *currentSection;
        }
    }
    throw machine_error(fmt::format("Unknown section {}", name));
}

void Machine::removeSection(std::string const& name)
{
    auto it = std::find_if(sections.begin(), sections.end(),
                           [&](auto const& s) { return s.name == name; });
    if (it != sections.end()) {
        sections.erase(it);
    }
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

void Machine::clear()
{
    if (fp != nullptr) {
        rewind(fp);
    }
    sections.clear();
    addSection("main", 0);
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
        "",
        "A",

        "#$%02x",
        "$%04x",

        "$%02x",
        "$%02x,x",
        "$%02x,y",
        "($%02x,x)",
        "($%02x),y",

        "($%04x)",
        "$%04x",
        "$%04x,x",
        "$%04x,y",
};
// clang-format on

void Machine::write(std::string const& name, OutFmt fmt)
{
    std::sort(sections.begin(), sections.end(),
              [](auto const& a, auto const& b) { return a.start < b.start; });

    int32_t last_end = -1;
    utils::File outFile{name, utils::File::Mode::Write};

    auto start = sections.front().start;
    auto end = sections.back().start + sections.back().data.size();

    if (end <= start) {
        printf("**Warning: No code generated\n");
        return;
    }

    if (fmt == OutFmt::Prg) {
        outFile.write<uint8_t>(start & 0xff);
        outFile.write<uint8_t>(start >> 8);
    }

    // bool bankFile = false;

    for (auto const& section : sections) {

        if ((int32_t)section.start < last_end) {
            throw machine_error(
                fmt::format("Section {} overlaps previous", section.name));
        }

        if (section.data.empty()) {
            continue;
        }

        if ((section.flags & WriteToDisk) != 0) {
            utils::File of{section.name, utils::File::Mode::Write};
            of.write<uint8_t>(section.start & 0xff);
            of.write<uint8_t>(section.start >> 8);
            of.write(section.data);
            of.close();
            continue;
        }

        if ((section.flags & NoStorage) != 0) {
            continue;
        }

        auto offset = section.start;
        auto adr = section.start & 0xffff;
        auto bank = section.start >> 16;

        if (bank > 0) {
            if (adr >= 0xa000 && adr + section.data.size() <= 0xc000) {
                offset = bank * 8192 + adr;
            } else {
                throw machine_error("Illegal address");
            }
        }
#if 0
        if (bank > 0 && !bankFile) {
            LOGI("Bank");
            outFile.close();
            outFile = utils::File("BANKED", utils::File::Mode::Write);
            outFile.write<uint8_t>(0);
            outFile.write<uint8_t>(0xa0);
            last_end = -1;
            bankFile = true;
        }
#endif
        if (last_end >= 0) {
            LOGI("Padding %d bytes", offset - last_end);
            while (last_end < (int)offset) {
                outFile.write<uint8_t>(0);
                last_end++;
            }
        }

        last_end = static_cast<uint32_t>(offset + section.data.size());

        LOGI("Writing %d bytes", section.data.size());
        outFile.write(section.data);
    }
}

uint32_t Machine::run(uint16_t pc)
{
    for (auto const& section : sections) {
        machine->writeRam(section.start, section.data.data(),
                          section.data.size());
    }
    machine->setPC(pc);
    return machine->run();
}

void Machine::setOutput(FILE* f)
{
    fp = f;
}

uint32_t Machine::writeByte(uint8_t w)
{
    currentSection->data.push_back(w);
    currentSection->pc++;
    return currentSection->pc;
}

uint32_t Machine::writeChar(uint8_t w)
{
    if (fp != nullptr) {
        if (!inData) {
            fprintf(fp, "%04x : \"", currentSection->pc);
        }
        inData = true;
        fputc(w, fp);
    }
    currentSection->data.push_back(w);
    currentSection->pc++;
    return currentSection->pc;
}

AsmResult Machine::assemble(Instruction const& instr)
{
    using sixfive::AddressingMode;

    auto arg = instr;
    auto opcode = instr.opcode;

    auto const& instructions = sixfive::Machine<>::getInstructions();

    auto it0 = std::find_if(instructions.begin(), instructions.end(),
                            [&](auto const& i) { return i.name == opcode; });

    if (it0 == instructions.end()) {
        return AsmResult::NoSuchOpcode;
    }

    auto it1 = std::find_if(
        it0->opcodes.begin(), it0->opcodes.end(), [&](auto const& o) {
            if (o.mode == AddressingMode::ZPX &&
                arg.mode == AddressingMode::ABSX && arg.val >= 0 &&
                arg.val <= 0xff) {
                arg.mode = AddressingMode::ZPX;
            }
            if (o.mode == AddressingMode::ZPY &&
                arg.mode == AddressingMode::ABSY && arg.val >= 0 &&
                arg.val <= 0xff) {
                arg.mode = AddressingMode::ZPY;
            }
            if (o.mode == AddressingMode::ZP &&
                arg.mode == AddressingMode::ABS && arg.val >= 0 &&
                arg.val <= 0xff) {
                arg.mode = AddressingMode::ZP;
            }
            if (o.mode == AddressingMode::REL) {
                arg.mode = AddressingMode::REL;
                arg.val =
                    arg.val - static_cast<int32_t>(currentSection->pc) - 2;
            }
            return o.mode == arg.mode;
        });

    if (it1 == it0->opcodes.end()) {
        return AsmResult::IllegalAdressingMode;
    }

    auto sz = opSize(arg.mode);

    auto v = arg.val & (sz == 2 ? 0xff : 0xffff);

    if (fp != nullptr) {
        if (inData) {
            fputs("\"\n", fp);
        }
        inData = false;
        fprintf(fp, "%04x : %s ", currentSection->pc, it0->name);
        if (arg.mode == sixfive::AddressingMode::REL) {
            v = (static_cast<int8_t>(v)) + 2 + currentSection->pc;
        }
        fprintf(fp, modeTemplate.at(arg.mode), v);
        fputs("\n", fp);
    }

    writeByte(it1->code);
    if (sz > 1) {
        writeByte(arg.val & 0xff);
    }
    if (sz > 2) {
        writeByte(arg.val >> 8);
    }

    if (arg.mode == AddressingMode::REL && (arg.val > 127 || arg.val < -128)) {
        return AsmResult::Truncated;
    }

    return AsmResult::Ok;
}

uint8_t Machine::readRam(uint16_t offset) const
{
    return machine->readMem(offset);
}
void Machine::writeRam(uint16_t offset, uint8_t val)
{
    machine->writeRam(offset, val);
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

void Machine::setBankWrite(int bank, int len,
                           std::function<void(uint16_t, uint8_t)> const& fn)
{
    bank_write_functions[bank] = fn;
    machine->mapWriteCallback(bank, len, this, bankWriteFunction);
}

void Machine::setBankRead(int bank, int len,
                          std::function<uint8_t(uint16_t)> const& fn)
{
    bank_read_functions[bank] = fn;
    machine->mapReadCallback(bank, len, this, bankReadFunction);
}

std::vector<uint8_t> Machine::getRam()
{
    std::vector<uint8_t> data(0x10000);
    machine->readRam(0, &data[0], data.size());
    return data;
}

Tuple6 Machine::getRegs() const
{
    return machine->regs();
}

void Machine::setRegs(Tuple6 const& regs)
{
    auto r = machine->regs();
    std::get<0>(r) = std::get<0>(regs);
    std::get<1>(r) = std::get<1>(regs);
    std::get<2>(r) = std::get<2>(regs);
    std::get<3>(r) = std::get<3>(regs);
    std::get<4>(r) = std::get<4>(regs);
    std::get<5>(r) = std::get<5>(regs);
}
