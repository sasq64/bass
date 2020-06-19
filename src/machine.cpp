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
    Machine* thiz = static_cast<Machine*>(data);

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
            //printf(fp, "SECTION %s\n", name.c_str());
        }
        auto it = std::find_if(sections.begin(), sections.end(),
                               [&](auto const& s) { return s.name == name; });
        if (it == sections.end()) {
            currentSection = &sections.emplace_back(name, start);
        } else {
            currentSection = &(*it);
            currentSection->start = currentSection->pc = start;
        }
    } else {
        currentSection = &sections.emplace_back("", start);
    }
    return *currentSection;
}

void Machine::setSection(std::string const& name)
{
    if (!name.empty()) {
        auto it = std::find_if(sections.begin(), sections.end(),
                               [&](auto const& s) { return s.name == name; });
        if (it != sections.end()) {
            currentSection = &(*it);
            return;
        }
    }
    throw machine_error(fmt::format("Unknown section {}", name));
}

Section const& Machine::getSection(std::string const& name) const
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

Machine::State Machine::saveState() const
{
    return {currentSection->pc, currentSection->data.size()};
}

void Machine::restoreState(State const& s)
{

    currentSection->pc = s.pc;
    currentSection->data.resize(s.offset);
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
    std::array<uint8_t, 64 * 1024> ram{};
    ram.fill(0);
    uint32_t start = 0xffffffff;
    uint32_t end = 0;
    for (auto const& section : sections) {

        if (section.data.empty()) {
            continue;
        }

        if ((section.flags & WriteToDisk) != 0) {
            utils::File of{section.name, utils::File::Mode::Write};
            of.write<uint8_t>(section.start & 0xff);
            of.write<uint8_t>(section.start >> 8);
            of.write(section.data);
            of.close();
        }

        if ((section.flags & NoStorage) != 0) {
            continue;
        }

        if (section.start < start) {
            start = section.start;
        }
        auto section_end =
            static_cast<uint32_t>(section.start + section.data.size());
        if (section_end > 64 * 1024) {
            section_end = 64 * 1024;
        }
        if (section_end > end) {
            end = section_end;
        }
        memcpy(&ram[section.start], section.data.data(),
               section_end - section.start);
    }

    if (end <= start) {
        printf("**Warning: No code generated\n");
        return;
    }

    if (end > start) {
        fmt::print("Writing {:04x}->{:04x} > {}\n", start, end, name);
        utils::File of{name, utils::File::Mode::Write};
        if (fmt == OutFmt::Prg) {
            of.write<uint8_t>(start & 0xff);
            of.write<uint8_t>(start >> 8);
        }
        of.write(&ram[start], end - start);
        of.close();
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
    /* if (fp != nullptr) { */
    /*     if(!inData) { */
    /*         fprintf(fp, "%04x : ", currentSection->pc); */
    /*     } */
    /*     fprintf(fp, "%02x ", w); */
    /*     inData = true; */
    /* } */
    currentSection->data.push_back(w);
    currentSection->pc++;
    return currentSection->pc;
}

uint32_t Machine::writeChar(uint8_t w)
{
    if (fp != nullptr) {
        if(!inData) {
            fprintf(fp, "%04x : \"", currentSection->pc);
        }
        inData = true;
        fputc(w, fp);
    }
    currentSection->data.push_back(w);
    currentSection->pc++;
    return currentSection->pc;
}

int Machine::assemble(Instruction const& instr)
{
    using sixfive::AdressingMode;

    auto arg = instr;
    auto opcode = instr.opcode;

    auto const& instructions = sixfive::Machine<>::getInstructions();

    auto it0 = std::find_if(instructions.begin(), instructions.end(),
                            [&](auto const& i) { return i.name == opcode; });

    if (it0 == instructions.end()) {
        return NO_SUCH_OPCODE;
    }

    auto diff = arg.val - static_cast<int32_t>(currentSection->pc) - 2;

    auto it1 = std::find_if(
        it0->opcodes.begin(), it0->opcodes.end(), [&](auto const& o) {
            if (o.mode == AdressingMode::ZPX &&
                arg.mode == AdressingMode::ABSX && arg.val >= 0 &&
                arg.val <= 0xff) {
                arg.mode = AdressingMode::ZPX;
            }
            if (o.mode == AdressingMode::ZPY &&
                arg.mode == AdressingMode::ABSY && arg.val >= 0 &&
                arg.val <= 0xff) {
                arg.mode = AdressingMode::ZPY;
            }
            if (o.mode == AdressingMode::ZP && arg.mode == AdressingMode::ABS &&
                arg.val >= 0 && arg.val <= 0xff) {
                arg.mode = AdressingMode::ZP;
            }
            if (o.mode == AdressingMode::REL &&
                arg.mode == AdressingMode::ABS && diff <= 127 && diff > -127) {
                arg.mode = AdressingMode::REL;
                arg.val = diff;
            }
            return o.mode == arg.mode;
        });

    if (it1 == it0->opcodes.end()) {
        return ILLEGAL_ADRESSING_MODE;
    }

    auto sz = opSize(arg.mode);

    auto v = arg.val & (sz == 2 ? 0xff : 0xffff);
    if (arg.mode == sixfive::AdressingMode::REL) {
        v = (static_cast<int8_t>(v)) + 2 + currentSection->pc;
    }

    if (fp != nullptr) {
        if(inData) {
            fputs("\"\n", fp);
        }
        inData = false;
        fprintf(fp, "%04x : %s ", currentSection->pc, it0->name);
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

    return sz;
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
    Machine* thiz = static_cast<Machine*>(data);
    thiz->bank_write_functions[adr >> 8](adr, val);
}

uint8_t Machine::bankReadFunction(uint16_t adr, void* data)
{
    Machine* thiz = static_cast<Machine*>(data);
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

std::tuple<unsigned, unsigned, unsigned, unsigned, unsigned, unsigned>
Machine::getRegs() const
{
    return machine->regs();
}
