#pragma once

#include "defines.h"
#include "wrap.h"

#include <coreutils/file.h>

#include <cstdint>
#include <deque>
#include <functional>
#include <map>
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

enum InterceptType
{
    None,
    Return,
    Call
};

struct Intercept
{
    enum InterceptType type { InterceptType::None };
    std::function<bool(uint32_t)> fn;
};


namespace sixfive {
struct DefaultPolicy;
template <class POLICY>
struct Machine;
} // namespace sixfive
struct EmuPolicy;

enum class AsmResult
{
    Ok,
    Truncated,
    NoSuchOpcode,
    IllegalAdressingMode,
    Failed
};

enum SectionFlags
{
    NoStorage = 1, // May not contain data (non leaf)
    WriteToDisk = 2,
    ReadOnly = 4,
    KeepFirst = 8,   // Keep first even if new First section is added
    KeepLast = 16,   // Keep last when new sections are added
    FixedStart = 32, // Section may not moved (specified with Start)
    FixedSize = 64   // Specified with size
};

struct Section
{
    Section() = default;
    Section(std::string const& n, uint32_t s) : name(n), start(s), pc(s) {}

    Section(std::string const& name_, std::string const& parent_) :
        name(name_), parent(parent_) {}

    Section& addByte(uint8_t b)
    {
        data.push_back(b);
        pc++;
        return *this;
    }

    Section& setStart(int32_t s)
    {
        start = s;
        return *this;
    }

    Section& setPC(int32_t s)
    {
        if (start == pc) {
            start = s;
        }
        pc = s;
        return *this;
    }

    std::string name;
    std::string parent;
    std::vector<std::string> children;
    int32_t start = -1;
    int32_t pc = -1;
    int32_t size = -1;
    uint32_t flags{};
    std::vector<uint8_t> data;
    bool valid{true};
};

enum class OutFmt
{
    Raw,
    Prg,
    Crt,
    EasyFlash,
};

class Machine
{
public:
    Machine();
    ~Machine();

    void clear();

    int32_t layoutSection(int32_t start, Section& s);
    bool layoutSections();
    Error checkOverlap();

    void writeCrt(utils::File const& outFile);

    uint32_t writeByte(uint8_t b);
    uint32_t writeChar(uint8_t b);
    AsmResult assemble(Instruction const& instr);
    std::string disassemble(uint32_t* pc);

    Section& addSection(Section const& s);

    void removeSection(std::string const& name);
    void setSection(std::string const& name);
    void popSection();
    void dropSection();
    void pushSection(std::string const& name);
    Section& getSection(std::string const& name);
    Section& getCurrentSection();
    std::deque<Section> const& getSections() const { return sections; }
    uint32_t getPC() const;
    void write(std::string_view name, OutFmt fmt);
    void writeListFile(std::string_view name);

    uint8_t readRam(uint16_t offset) const;
    void writeRam(uint16_t offset, uint8_t val);

    uint32_t run(uint16_t pc);
    uint32_t go(uint16_t pc);
    void runSetup();
    std::vector<uint8_t> getRam();

    unsigned getReg(sixfive::Reg reg);
    void setReg(sixfive::Reg reg, unsigned v);
    void setRegs(RegState const& regs);

    //void setBreakFunction(uint8_t what, std::function<void(uint8_t)> const& fn);
    void addIntercept(uint32_t address, std::function<bool(uint32_t)> const& fn);

    static void bankWriteFunction(uint16_t adr, uint8_t val, void* data);
    static uint8_t bankReadFunction(uint16_t adr, void* data);

    void setBankWrite(int hi_adr, int len,
                      std::function<void(uint16_t, uint8_t)> const& fn);
    void setBankRead(int hi_adr, int len,
                     std::function<uint8_t(uint16_t)> const& fn);
    void setBankRead(int hi_adr, int len, int bank);

    enum CPU
    {
        CPU_6502,
        CPU_65C02
    };

    void setCpu(CPU cpu);

    std::map<uint32_t, std::string> dis;

private:

    bool cpu65C02 = true;

    std::deque<Section*> savedSections;
    //bool inData = false;
    std::unordered_map<uint8_t, std::function<uint8_t(uint16_t)>>
        bank_read_functions;
    std::unordered_map<uint8_t, std::function<void(uint16_t, uint8_t)>>
        bank_write_functions;

    std::unique_ptr<sixfive::Machine<EmuPolicy>> machine;
    std::deque<Section> sections;
    Section* currentSection = nullptr;
    int anonSection = 0;

    bool layoutOk{false};
};
