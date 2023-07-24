#pragma once

#include "defines.h"
#include "parser.h"

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
    FixedStart = 32, // Section may not moved (specified with Start)
    FixedSize = 64,  // Specified with size
    Compressed = 128,
    Backwards = 256,
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
        pc = pc.value() + 1;
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

    int32_t get_size() const
    {
        return size >= 0 ? size.value() : (int32_t)data.size();
    }

    std::string name;
    std::string parent;
    std::vector<std::string> children;
    Optional<int32_t> start = -1;
    Optional<int32_t> pc = -1;
    Optional<int32_t> size = -1;
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
    PackedPrg
};

class Machine
{
public:
    explicit Machine(uint32_t start = 0);
    ~Machine();

    void clear();

    int32_t layoutSection(Optional<int32_t> const& o_start, Section& s);
    bool layoutSections();
    Error checkOverlap();

    void writeCrt(utils::File const& outFile);

    uint32_t writeByte(uint8_t b);
    uint32_t writeChar(uint8_t b);
    AsmResult assemble(Instruction const& instr);
    static std::string disassemble(sixfive::Machine<EmuPolicy>& m, uint32_t* pc);

    Section& addSection(Section const& s);

    void removeSection(std::string const& name);
    void setSection(std::string const& name);
    void popSection();
    void dropSection();
    void pushSection(std::string const& name);
    Section& getSection(std::string const& name);
    Section& getCurrentSection();
    std::deque<Section> const& getSections() const { return sections; }
    void sortSectionsByStart() {
        std::sort(sections.begin(), sections.end(), [](auto a, auto b){
            return a.start < b.start;
        });
    }
    uint32_t getPC() const;
    void write(std::string_view name, OutFmt fmt);
    void writeListFile(std::string_view name);

    uint8_t readRam(uint16_t offset) const;
    uint8_t readMem(uint16_t adr) const;
    void writeRam(uint16_t offset, uint8_t val);
    void writeRam(uint16_t org, const uint8_t* data, size_t size);

    uint32_t run(uint16_t pc);
    uint32_t go(uint16_t pc);

    void setPC(uint32_t pc);
    bool runCycles(uint32_t cycles);

    std::pair<uint32_t, uint32_t> runSetup();
    std::vector<uint8_t> getRam();

    unsigned getReg(sixfive::Reg reg);
    void setReg(sixfive::Reg reg, unsigned v);
    void setRegs(RegState const& regs);
    void doIrq(uint16_t adr);

    void addJsrFunction(uint32_t address, std::function<void(uint32_t)> const& fn);

    void mapFile(const std::string& name, uint8_t bank);

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

    void SetTracing(bool b);

private:

    static bool modeMatches(sixfive::Mode ourMode, sixfive::Mode targetMode, bool small);

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
