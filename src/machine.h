#pragma once

#include "defines.h"

#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

class machine_error : public std::exception
{
public:
    explicit machine_error(std::string m = "Target error") : msg(std::move(m))
    {}
    const char* what() const noexcept override { return msg.c_str(); }

private:
    std::string msg;
};

namespace sixfive {
struct DefaultPolicy;
template <class POLICY>
struct Machine;
} // namespace sixfive

enum SectionFlags
{
    NoStorage = 1,
    WriteToDisk = 2,
    ReadOnly = 4
};

enum class AsmResult
{
    Ok,
    Truncated,
    NoSuchOpcode,
    IllegalAdressingMode,
    Failed
};

struct Section
{
    Section(std::string const& n, uint16_t s) : name(n), start(s), pc(s) {}
    std::string name;
    uint32_t start = 0;
    uint32_t pc = 0;
    uint32_t flags{};
    std::vector<uint8_t> data;
};

enum class OutFmt
{
    Prg,
    Raw
};

using Tuple6 = std::tuple<unsigned, unsigned, unsigned, unsigned, unsigned, unsigned>;

class Machine
{
public:

    Machine();
    ~Machine();

    void clear();

    uint32_t writeByte(uint8_t b);
    uint32_t writeChar(uint8_t b);
    AsmResult assemble(Instruction const& instr);
    Section& addSection(std::string const& name, uint32_t start);
    void setSection(std::string const& name);
    Section const& getSection(std::string const& name) const;
    Section& getCurrentSection();
    std::deque<Section> const& getSections() const { return sections; }
    uint32_t getPC() const;
    void write(std::string const& name, OutFmt fmt);
    void setOutput(FILE* f);

    uint8_t readRam(uint16_t offset) const;
    void writeRam(uint16_t offset, uint8_t val);

    uint32_t run(uint16_t pc);
    std::vector<uint8_t> getRam();


    Tuple6 getRegs() const;
    void setRegs(Tuple6 const& regs);

    void setBreakFunction(uint8_t what, std::function<void(uint8_t)> const& fn);

    static void bankWriteFunction(uint16_t adr, uint8_t val, void* data);
    static uint8_t bankReadFunction(uint16_t adr, void* data);

    void setBankWrite(int bank, int len,
                      std::function<void(uint16_t, uint8_t)> const& fn);
    void setBankRead(int bank, int len,
                     std::function<uint8_t(uint16_t)> const& fn);

private:
    bool inData = false;
    std::unordered_map<uint8_t, std::function<void(uint8_t)>> break_functions;
    std::unordered_map<uint8_t, std::function<uint8_t(uint16_t)>>
        bank_read_functions;
    std::unordered_map<uint8_t, std::function<void(uint16_t, uint8_t)>>
        bank_write_functions;

    static void breakFunction(int what, void* data);

    std::unique_ptr<sixfive::Machine<sixfive::DefaultPolicy>> machine;
    std::deque<Section> sections;
    Section* currentSection = nullptr;
    FILE* fp = nullptr;
};
