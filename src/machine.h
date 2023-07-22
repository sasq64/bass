#pragma once

#include "defines.h"
#ifdef USE_BASS_VALUEPROVIDER
#   include "valueprovider.h"
#endif
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


#ifdef USE_BASS_VALUEPROVIDER

// TODO: SectionInitializer is required for transition from SymbolTable!
//       -> see "meta.h -> ::parseArgs"
//          creates a temporary Section instance being used to
//          create the actual section instance "Machine::addSection".
struct SectionInitializer final {
    SectionInitializer() = default;
    SectionInitializer(std::string const& n, uint32_t s)
        : name(n)
        , start(s)
    {}

    std::string name;
    std::string parent;
    std::vector<std::string> children;
    int32_t start = -1;  // TODO: std::optional<int32_t>
    int32_t pc = -1;
    int32_t size = -1;
    uint32_t flags{};
    std::vector<uint8_t> data;
    bool valid{true};
};

struct Section
{
    Section() = delete;
    Section(std::string const& n, std::optional<int32_t> const& s = {})
        : name(n)
        , mvp(n.empty() ? "Anonymous Section" : n)
    {
        setup(s);
    }
    Section(std::string const& name_, std::string const& parent_)
        : name(name_)
        , parent(parent_)
        , mvp(name_.empty() ? "Anonymous Section" : name_)
    {
        setup({});
    }
    ~Section() = default;

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

    int32_t get_size() const
    {
        return size >= 0 ? size : (int32_t)data.size();
    }

    inline void setup(std::optional<int32_t> const& s)
    {
        if (name.empty()) {
            return;
        }

        if (initialized) {
            auto err_msg = fmt::format("Section {} already initialized", name);
            LOGD(err_msg);
            throw std::runtime_error(err_msg);
        }

        auto prefix{fmt::format("section.{}", name)};

        mvp.setupValues({
            fmt::format("{}.start", prefix),
            fmt::format("{}.end", prefix),
            fmt::format("{}.data", prefix),
            fmt::format("{}.pc", prefix),
            fmt::format("{}.original_size", prefix),  // uncompressed data size
            fmt::format("{}.size", prefix),
        });

        { // FIXME: deprecated (see meta.cpp -> assem.registerMeta("section"))
            auto prefix{fmt::format("sections.{}", name)};
            mvp.setupValues({
                fmt::format("{}.start", prefix),
                fmt::format("{}.end", prefix),
                fmt::format("{}.data", prefix),
                fmt::format("{}.pc", prefix),
                fmt::format("{}.original_size", prefix),  // uncompressed data size
                fmt::format("{}.size", prefix),
            });
        }

        if (s.has_value()) {
            start = s.value();
            pc = s.value();
        }

        initialized = true;
    }

    void storeOriginalSize()
    {
        // NOTE: just stores the uncompressed section data size to MVP
        // TODO: compress the section data here (see meta.cpp -> assem.registerMeta("section"))
        auto prefix = fmt::format("section.{}", name);
        mvp.setValue(fmt::format("{}.original_size", prefix), std::optional{int32_t(data.size())});

        {  // deprecated (see meta.cpp -> assem.registerMeta("section"))
            auto prefix = fmt::format("sections.{}", name);
            mvp.setValue(fmt::format("{}.original_size", prefix), std::optional{int32_t(data.size())});
        }
    }

    void storeSymbols()
    {
        auto start_ = start > -1 ? std::optional{start} : std::nullopt;
        auto end_   = start_.has_value() ? std::optional{int32_t(start + data.size())} : std::nullopt;
        auto pc_    = pc > -1 ? std::optional{pc} : std::nullopt;
        auto size_  = std::optional{int32_t(data.size())};

        auto prefix{fmt::format("section.{}", name)};
        mvp.setValue(fmt::format("{}.start", prefix), start_);
        mvp.setValue(fmt::format("{}.end", prefix), end_);
        mvp.setValue(fmt::format("{}.data", prefix), std::optional{data});
        mvp.setValue(fmt::format("{}.pc", prefix), pc_);
        mvp.setValue(fmt::format("{}.size", prefix), size_);
        auto original_size_key = fmt::format("{}.original_size", prefix);
        if (!mvp.hasValue(original_size_key)) {
            mvp.setValue(original_size_key, size_);
        }

        { // FIXME: deprecated (see meta.cpp -> assem.registerMeta("section"))
            auto prefix{fmt::format("sections.{}", name)};
            mvp.setValue(fmt::format("{}.start", prefix), start_);
            mvp.setValue(fmt::format("{}.end", prefix), end_);
            mvp.setValue(fmt::format("{}.data", prefix), std::optional{data});
            mvp.setValue(fmt::format("{}.pc", prefix), pc_);
            mvp.setValue(fmt::format("{}.size", prefix), size_);
            auto original_size_key = fmt::format("{}.original_size", prefix);
            if (!mvp.hasValue(original_size_key)) {
                mvp.setValue(original_size_key, size_);
            }
        }
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

private:
    MultiValueProvider mvp;
    bool initialized = false;
};
#else
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

    int32_t get_size() const
    {
        return size >= 0 ? size : (int32_t)data.size();
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

using SectionInitializer = Section;

#endif

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

    int32_t layoutSection(int32_t start, Section& s);
    bool layoutSections();
    Error checkOverlap();

    void writeCrt(utils::File const& outFile);

    uint32_t writeByte(uint8_t b);
    uint32_t writeChar(uint8_t b);
    AsmResult assemble(Instruction const& instr);
    static std::string disassemble(sixfive::Machine<EmuPolicy>& m, uint32_t* pc);

    Section& addSection(SectionInitializer const& s);

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
