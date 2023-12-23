#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <limits>
#include <tuple>
#include <type_traits>
#include <vector>

namespace sixfive {

enum class Mode : uint8_t
{
    NONE,
    ACC,
    IMM,
    REL,
    ZP,
    ZPX,
    ZPY,
    INDX,
    INDY,
    INDZ,
    IND,
    ABS,
    ABSX,
    ABSY,
    ZP_REL,
    ADR, // Undecided ABS or ZP
    ADRX, // Undecided ABS or ZP
    ADRY, // Undecided ABS or ZP
};

enum class Reg
{
    A,
    X,
    Y,
    SP,
    SR,
    PC
};

static constexpr inline int opSize(Mode am)
{
    return am >= Mode::IND ? 3 : (am >= Mode::IMM ? 2 : 1);
}

template <typename POLICY>
struct Machine;

enum EmulatedMemoryAccess
{
    Direct,  // Access `ram` array directly; Means no bank switching, ROM areas
             // or IO areas
    Banked,  // Access memory through `wbank` and `rbank`; Means no IO areas
    Callback // Access memory via function pointer per bank
};

// The Policy defines the compile & runtime time settings for the emulator
struct DefaultPolicy
{
    DefaultPolicy() = default;
    explicit DefaultPolicy(Machine<DefaultPolicy>&) {}


    // Use switch/case statement instead of jump table. Quite a bit faster but
    // limits the emulation to a fixed opcode setup (6502 + illegals).
    static constexpr bool UseSwitch = true;

    // Return from run() if "rts" is executed when there is nothing on the stack
    static constexpr bool ExitOnStackWrap = true;

    // PC accesses does not normally need to work in IO areas
    static constexpr int PC_AccessMode = Banked;

    // Generic reads and writes should normally not be direct
    static constexpr int Read_AccessMode = Callback;
    static constexpr int Write_AccessMode = Callback;

    static constexpr int MemSize = 65536;

    static constexpr bool breakFn(DefaultPolicy&, int /*n*/) { return false; }
    static constexpr uint32_t jsrMask = 0;
    static constexpr bool jsrFn(DefaultPolicy&, uint32_t /*target*/)
    {
        return false;
    }

    // This function is run after each opcode. Return true to stop emulation.
    static constexpr bool eachOp(DefaultPolicy&) { return false; }
    static constexpr void afterRun(DefaultPolicy&) {}

    static constexpr void ioWrite(DefaultPolicy&, uint8_t v) {}
};

template <typename POLICY = DefaultPolicy>
struct Machine
{
    using Adr = uint16_t;
    using Word = uint8_t;

    using OpFunc = void (*)(Machine&);

    struct Opcode
    {
        Opcode() = default;
        Opcode(uint8_t code, int cycles, Mode mode, OpFunc op)
            : code(code), cycles(cycles), mode(mode), op(op)
        {
        }
        uint8_t code = 0;
        uint8_t cycles = 0;
        Mode mode = Mode::NONE;
        OpFunc op = nullptr;
    };

    struct Instruction
    {
        Instruction(char const* name, std::vector<Opcode> ov)
            : name(name), opcodes(std::move(ov))
        {
        }
        char const* name;
        std::vector<Opcode> opcodes;
    };

    ~Machine() = default;
    Machine(Machine&& op) noexcept = default;
    Machine& operator=(Machine&& op) noexcept = default;

    Opcode illegal_opcode = {0xff, 0, Mode::IMM, [](Machine& m) {
                                 // fmt::print("** Illegal opcode at {:04x}\n",
                                 // m.regPC());
                                 m.realCycles = m.cycles;
                                 m.cycles =
                                     std::numeric_limits<uint32_t>::max() - 32;
                             }};

    Machine() : policy_{*this}
    {
        ram.fill(0);
        stack = &ram[0x100];
        for (int i = 0; i < 256; i++) {
            rbank.at(i) = wbank.at(i) = &ram[(i * 256) % POLICY::MemSize];
            rcallbacks.at(i) = &read_bank;
            rcbdata.at(i) = this;
            wcallbacks.at(i) = &write_bank;
            wcbdata.at(i) = this;
        }
        if constexpr (!POLICY::UseSwitch) {
            jumpTable = jumpTable_normal.data();
            set_cpu(false);
        }
    }

    void set_cpu(bool cpu6502)
    {
        static_assert(!POLICY::UseSwitch, "Can't set cpu when using switch/case version");

        for (int i = 0; i < 256; i++) {
            jumpTable_normal[i] = illegal_opcode;
            jumpTable_bcd[i] = illegal_opcode;
        }
        for (auto const& i : getInstructions<false>(cpu6502)) {
            for (auto const& o : i.opcodes)
                jumpTable_normal[o.code] = o;
        }
        for (auto const& i : getInstructions<true>(cpu6502)) {
            for (auto const& o : i.opcodes)
                jumpTable_bcd[o.code] = o;
        }
    }

    POLICY policy_;

    POLICY& policy()
    {
        // static POLICY policy(*this);
        return policy_;
    }

    // Access ram directly

    [[nodiscard]] Word const& get_stack(Word const& adr) const
    {
        return stack[adr];
    }

    void write_ram(uint16_t org, Word const data) { ram[org] = data; }

    void clear_ram() { std::fill(ram.begin(), ram.end(), 0); }

    void write_ram(uint16_t org, uint8_t const* data, size_t size)
    {
        for (size_t i = 0; i < size; i++)
            ram[org + i] = data[i];
    }

    void write_memory(uint16_t org, uint8_t const* data, size_t size)
    {
        for (size_t i = 0; i < size; i++)
            Write(org + i, data[i]);
    }

    void read_ram(uint16_t org, uint8_t* data, size_t size) const
    {
        for (size_t i = 0; i < size; i++)
            data[i] = ram[org + i];
    }

    [[nodiscard]] uint8_t read_ram(uint16_t org) const { return ram[org]; }

    // Access memory through bank mapping

    [[nodiscard]] uint8_t read_mem(uint16_t org) const { return Read(org); }

    void read_mem(uint16_t org, uint8_t* data, int size) const
    {
        for (int i = 0; i < size; i++)
            data[i] = read_mem(org + i);
    }

    // Map ROM to a bank
    void map_rom(uint8_t bank, Word const* data, int len)
    {
        auto const* end = data + len;
        while (data < end) {
            rbank[bank++] = const_cast<Word*>(data); // NOLINT
            data += 256;
        }
    }

    void map_read_callback(uint8_t bank, int len, void* data,
                           uint8_t (*cb)(uint16_t, void*))
    {
        while (len > 0) {
            rcallbacks[bank] = cb;  // NOLINT
            rcbdata[bank++] = data; // NOLINT
            len--;
        }
    }
    void map_write_callback(uint8_t bank, int len, void* data,
                            void (*cb)(uint16_t, uint8_t, void*))
    {
        while (len > 0) {
            wcallbacks[bank] = cb;  // NOLINT
            wcbdata[bank++] = data; // NOLINT
            len--;
        }
    }

    void irq(unsigned adr)
    {
        stack[sp] = pc >> 8;
        stack[sp - 1] = pc & 0xff;
        stack[sp - 2] = get_SR();
        sr |= 4;
        sp -= 3;
        pc = adr;
    }

    template <enum Reg REG>
    [[nodiscard]] unsigned get() const
    {
        return Reg<REG>();
    }
    template <enum Reg REG>
    void set(unsigned v)
    {
        if constexpr (REG == Reg::SR) { set_SR(v); }
        else { Reg<REG>() = v; }
    }

    [[nodiscard]] Adr regPC() const { return pc; }
    void setPC(uint16_t const& p) { pc = p; }

    uint32_t run(uint32_t toCycles = 0x010000000)
    {
        if constexpr (POLICY::UseSwitch) {
            return run_switch_case(toCycles);
        }
        return run_jumptable(toCycles);
    }

    auto regs() const { return std::make_tuple(a, x, y, get_SR(), sp, pc); }
    //auto regs() { return std::tie(a, x, y, get_SR(), sp, pc); }

    template <bool USE_BCD = false>
    static auto const& getInstructions()
    {
        static std::vector<Instruction> const instructionTable =
            makeInstructionTable<USE_BCD>();
        return instructionTable;
    }

    template <bool USE_BCD = false>
    static auto const& getInstructions(bool cpu65c02)
    {
        static std::vector<Instruction> const instructionTable =
            makeInstructionTable<USE_BCD>(true);
        static std::vector<Instruction> const instructionTable2 =
            makeInstructionTable<USE_BCD>(false);
        return cpu65c02 ? instructionTable : instructionTable2;
    }

private:
    // The 6502 registers
    unsigned pc{0};
    unsigned a{0};
    unsigned x{0};
    unsigned y{0};

    // Status Register _except_for S and Z flags
    unsigned sr{0x30};
    // Result of last operation; Used for S and Z flags.
    // result & 0xff == 0 => Z flag is set
    // result & 0x280 != 0 => S flag is set
    unsigned result{0};

    uint8_t sp{0xff};

    uint32_t cycles = 0;
    uint32_t realCycles = 0;

    // Current jumptable
    Opcode const* jumpTable;

    // Stack normally points to ram[0x100];
    Word* stack;

    // 6502 RAM
    std::array<Word, POLICY::MemSize> ram;

    // BreakFn breakFunction = nullptr;
    // void* breakData = nullptr;

    // Banks normally point to corresponding ram
    std::array<Word const*, 256> rbank{};
    std::array<Word*, 256> wbank{};

    std::array<Word (*)(uint16_t, void*), 256> rcallbacks{};
    std::array<void*, 256> rcbdata{};
    std::array<void (*)(uint16_t, Word, void*), 256> wcallbacks{};
    std::array<void*, 256> wcbdata{};

    std::array<Opcode, 256> jumpTable_normal;
    std::array<Opcode, 256> jumpTable_bcd;

    static void write_bank(uint16_t adr, Word v, void* ptr)
    {
        auto* m = static_cast<Machine*>(ptr);
        m->wbank[adr >> 8][adr & 0xff] = v & 0xff;
    }

    static Word read_bank(uint16_t adr, void* ptr)
    {
        auto* m = static_cast<Machine*>(ptr);
        return m->rbank[adr >> 8][adr & 0xff];
    }

    template <enum Reg REG>
    constexpr auto Reg() const
    {
        if constexpr (REG == Reg::A) return a;
        if constexpr (REG == Reg::X) return x;
        if constexpr (REG == Reg::Y) return y;
        if constexpr (REG == Reg::SR) return get_SR();
        if constexpr (REG == Reg::SP) return sp;
        if constexpr (REG == Reg::PC) return pc;
    }

    template <enum Reg REG>
    constexpr auto& Reg()
    {
        if constexpr (REG == Reg::A) return a;
        if constexpr (REG == Reg::X) return x;
        if constexpr (REG == Reg::Y) return y;
        if constexpr (REG == Reg::SR) return get_SR();
        if constexpr (REG == Reg::SP) return sp;
        if constexpr (REG == Reg::PC) return pc;
    }
    /////////////////////////////////////////////////////////////////////////
    ///
    /// THE STATUS REGISTER
    ///
    /////////////////////////////////////////////////////////////////////////

    // S V - b d i Z C
    // 1 1 0 0 0 0 1 1

    enum STATUS_FLAGS
    {
        CARRY,
        ZERO,
        IRQ,
        DECIMAL,
        BRK,
        xXx,
        OVER,
        SIGN
    };

    enum STATUS_BITS
    {
        C = 0x1,
        Z = 0x2,
        d_FLAG = 0x8,
        V = 0x40,
        S = 0x80,
    };

    static constexpr auto SZ = S | Z;
    static constexpr auto SZC = S | Z | C;
    static constexpr auto SZCV = S | Z | C | V;

    [[nodiscard]] uint8_t get_SR() const
    {
        return sr | ((result | (result >> 2)) & 0x80) | ((result & 0xff) == 0 ? 2 : 0);
    }
    
    bool decMode = false;

    template <bool DEC>
    void setDec()
    {
        if constexpr (POLICY::UseSwitch) {
            decMode = DEC;
        } else {
            if constexpr (DEC)
                jumpTable = jumpTable_bcd.data();
            else
                jumpTable = jumpTable_normal.data();
        }
    }

    void set_SR(uint8_t s)
    {
        if ((s ^ sr) & d_FLAG) { // D bit changed
            if (s & d_FLAG) {
                setDec<true>();
            } else {
                setDec<false>();
            }
        }
        result = ((s << 2) & 0x200) | !(s & Z);
        sr = (s & ~SZ) | 0x30;
    }

    template <int BITS>
    void set(int res, int arg = 0)
    {
        result = res;

        if constexpr ((BITS & (C | V)) != 0) sr &= ~BITS;
        if constexpr ((BITS & C) != 0) sr |= ((res >> 8) & 1); // Apply carry
        if constexpr ((BITS & V) != 0)
            sr |= ((~(a ^ arg) & (a ^ res) & 0x80) >> 1); // Apply overflow
    }

    static constexpr bool SET = true;
    static constexpr bool CLEAR = false;

    [[nodiscard]] constexpr unsigned carry() const { return sr & 1; }

    template <int FLAG, bool v>
    [[nodiscard]] constexpr bool check() const
    {
        if constexpr (FLAG == ZERO) return ((result & 0xff) != 0) == !v;
        if constexpr (FLAG == SIGN)
            return ((result & 0x280) != 0) == v;
        else
            return ((sr & 1 << FLAG) != 0) == v;
    }

    /////////////////////////////////////////////////////////////////////////
    ///
    /// MEMORY ACCESS
    ///
    /////////////////////////////////////////////////////////////////////////

    // Get low byte from an address
    static constexpr unsigned lo(unsigned a) { return a & 0xff; }
    // Get high byte from an address
    static constexpr unsigned hi(unsigned a) { return a >> 8; }

    // Form an address from lo and hi byte
    static constexpr unsigned to_adr(unsigned lo, unsigned hi)
    {
        return (hi << 8) | lo;
    }

    uint64_t rmask = 0;
    uint64_t wmask = 0;

    template <int ACCESS_MODE = POLICY::Read_AccessMode>
    [[nodiscard]] unsigned Read(unsigned adr) const
    {
        if constexpr (ACCESS_MODE == Direct)
            return ram[adr];
        else if constexpr (ACCESS_MODE == Banked)
            return rbank[hi(adr)][lo(adr)];
        else {
            auto h = hi(adr);
            if ((1<<(h>>4)) & rmask) {
                return rcallbacks[h](adr, rcbdata[hi(adr)]);
            } else {
                return rbank[h][lo(adr)];
            }
        }
    }

    template <int ACCESS_MODE = POLICY::Write_AccessMode>
    void Write(unsigned adr, unsigned v)
    {
        if(adr == 1) {
           // 6510 I/O register
           ram[adr] = v;
           POLICY::ioWrite(policy(), v);
           return;
        }
        if constexpr (ACCESS_MODE == Direct)
            ram[adr] = v;
        else if constexpr (ACCESS_MODE == Banked)
            wbank[hi(adr)][lo(adr)] = v;
        else {
            wcallbacks[hi(adr)](adr, v, wcbdata[hi(adr)]);
        }
    }

    unsigned ReadPC() { return Read<POLICY::PC_AccessMode>(pc++); }

    unsigned ReadPC8(unsigned offs = 0)
    {
        return (Read<POLICY::PC_AccessMode>(pc++) + offs) & 0xff;
    }

    unsigned ReadPC16(unsigned offs = 0)
    {
        auto adr = to_adr(Read<POLICY::PC_AccessMode>(pc),
                          Read<POLICY::PC_AccessMode>(pc + 1));
        pc += 2;
        return adr + offs;
    }

    [[nodiscard]] unsigned Read16(unsigned adr, unsigned offs = 0) const
    {
        return to_adr(Read(adr), Read(adr + 1)) + offs;
    }

    // Read operand from PC and create effective address depending on 'MODE'
    template <enum Mode MODE>
    unsigned ReadEA()
    {
        if constexpr (MODE == Mode::IMM) return pc++;
        if constexpr (MODE == Mode::ZP) return ReadPC8();
        if constexpr (MODE == Mode::ZPX) return ReadPC8(x);
        if constexpr (MODE == Mode::ZPY) return ReadPC8(y);
        if constexpr (MODE == Mode::ABS) return ReadPC16();
        if constexpr (MODE == Mode::ABSX) return ReadPC16(x);
        if constexpr (MODE == Mode::ABSY) return ReadPC16(y);
        if constexpr (MODE == Mode::INDX) return Read16(ReadPC8(x));
        if constexpr (MODE == Mode::INDY) return Read16(ReadPC8(), y);
        if constexpr (MODE == Mode::IND) return Read16(ReadPC16());
        if constexpr (MODE == Mode::INDZ) return Read16(ReadPC8());
    }

    template <enum Mode MODE>
    void StoreEA(unsigned v)
    {
        Write(ReadEA<MODE>(), v);
    }

    template <enum Mode MODE>
    unsigned LoadEA()
    {
        return Read(ReadEA<MODE>());
    }

    /////////////////////////////////////////////////////////////////////////
    ///
    ///   OPCODES
    ///
    /////////////////////////////////////////////////////////////////////////

    // Set flags, except for Z and S
    template <int FLAG, bool ON>
    static constexpr void Set(Machine& m)
    {
        if constexpr (FLAG == DECIMAL) m.setDec<ON>();
        m.sr = (m.sr & ~(1 << FLAG)) | (ON << FLAG);
    }

    template <enum Reg REG, enum Mode MODE>
    static constexpr void Store(Machine& m)
    {
        m.StoreEA<MODE>(m.Reg<REG>());
    }

    template <enum Mode MODE>
    static constexpr void Store0(Machine& m)
    {
        m.StoreEA<MODE>(0);
    }

    template <enum Reg REG, enum Mode MODE>
    static constexpr void Load(Machine& m)
    {
        m.Reg<REG>() = m.LoadEA<MODE>();
        m.set<SZ>(m.Reg<REG>());
    }

    template <int FLAG, bool ON>
    static constexpr void Branch(Machine& m)
    {
        auto pc = m.pc;
        int8_t diff = m.Read<POLICY::PC_AccessMode>(pc++);
        if (m.check<FLAG, ON>()) {
            pc += diff;
            m.cycles++;
        }
        m.pc = pc;
    }
    static constexpr void Branch(Machine& m)
    {
        auto pc = m.pc;
        int8_t diff = m.Read<POLICY::PC_AccessMode>(pc++);
        pc += diff;
        m.cycles++;
        m.pc = pc;
    }

    template <enum Mode MODE, int INC>
    static constexpr void Inc(Machine& m)
    {
        auto adr = m.ReadEA<MODE>();
        auto rc = (m.Read(adr) + INC);
        m.Write(adr, rc);
        m.set<SZ>(rc);
    }

    template <enum Reg REG, int INC>
    static constexpr void Inc(Machine& m)
    {
        m.Reg<REG>() = (m.Reg<REG>() + INC) & 0xff;
        m.set<SZ>(m.Reg<REG>());
    }

    // === COMPARE, ADD & SUBTRACT

    template <enum Mode MODE>
    static constexpr void Bit(Machine& m)
    {
        unsigned z = m.LoadEA<MODE>();
        m.result = (z & m.a) | ((z & 0x80) << 2);
        m.sr = (m.sr & ~V) | (z & V);
    }

    template <enum Reg REG, enum Mode MODE>
    static constexpr void Cmp(Machine& m)
    {
        unsigned z = (~m.LoadEA<MODE>()) & 0xff;
        unsigned rc = m.Reg<REG>() + z + 1;
        m.set<SZC>(rc);
    }

    template <enum Mode MODE, bool DEC = false>
    static constexpr void Sbc(Machine& m)
    {
        if constexpr (DEC) {
            unsigned z = m.LoadEA<MODE>();
            auto al = (m.a & 0xf) - (z & 0xf) + (m.carry() - 1);
            auto ah = (m.a >> 4) - (z >> 4);
            if (al & 0x10) {
                al = (al - 6) & 0xf;
                ah--;
            }
            if (ah & 0x10) ah = (ah - 6) & 0xf;
            unsigned rc = m.a - z + (m.carry() - 1);
            m.set<SZCV>(rc ^ 0x100, z);
            m.a = al | (ah << 4);
        } else {
            unsigned z = (~m.LoadEA<MODE>()) & 0xff;
            unsigned rc = m.a + z + m.carry();
            m.set<SZCV>(rc, z);
            m.a = rc & 0xff;
        }
    }

    template <enum Mode MODE, bool DEC = false>
    static constexpr void Adc(Machine& m)
    {
        unsigned z = m.LoadEA<MODE>();
        unsigned rc = m.a + z + m.carry();
        if constexpr (DEC) {
            if (((m.a & 0xf) + (z & 0xf) + m.carry()) >= 10) rc += 6;
            if ((rc & 0xff0) > 0x90) rc += 0x60;
        }
        m.set<SZCV>(rc, z);
        m.a = rc & 0xff;
    }

    template <enum Mode MODE>
    static constexpr void And(Machine& m)
    {
        m.a &= m.LoadEA<MODE>();
        m.set<SZ>(m.a);
    }

    template <enum Mode MODE>
    static constexpr void Ora(Machine& m)
    {
        m.a |= m.LoadEA<MODE>();
        m.set<SZ>(m.a);
    }

    template <enum Mode MODE>
    static constexpr void Eor(Machine& m)
    {
        m.a ^= m.LoadEA<MODE>();
        m.set<SZ>(m.a);
    }

    // === SHIFTS & ROTATES

    template <enum Mode MODE>
    static constexpr void Asl(Machine& m)
    {
        if constexpr (MODE == Mode::ACC) {
            int rc = m.a << 1;
            m.set<SZC>(rc);
            m.a = rc & 0xff;
        } else {
            auto adr = m.ReadEA<MODE>();
            unsigned rc = m.Read(adr) << 1;
            m.set<SZC>(rc);
            m.Write(adr, rc);
        }
    }

    template <enum Mode MODE>
    static constexpr void Lsr(Machine& m)
    {
        if constexpr (MODE == Mode::ACC) {
            m.sr = (m.sr & 0xfe) | (m.a & 1);
            m.a >>= 1;
            m.set<SZ>(m.a);
        } else {
            auto adr = m.ReadEA<MODE>();
            unsigned rc = m.Read(adr);
            m.sr = (m.sr & 0xfe) | (rc & 1);
            rc >>= 1;
            m.Write(adr, rc);
            m.set<SZ>(rc);
        }
    }

    template <enum Mode MODE>
    static constexpr void Ror(Machine& m)
    {
        if constexpr (MODE == Mode::ACC) {
            unsigned rc = (((m.sr << 8) & 0x100) | m.a) >> 1;
            m.sr = (m.sr & 0xfe) | (m.a & 1);
            m.a = rc & 0xff;
            m.set<SZ>(m.a);
        } else {
            auto adr = m.ReadEA<MODE>();
            unsigned rc = m.Read(adr) | ((m.sr << 8) & 0x100);
            m.sr = (m.sr & 0xfe) | (rc & 1);
            rc >>= 1;
            m.Write(adr, rc);
            m.set<SZ>(rc);
        }
    }

    template <enum Mode MODE>
    static constexpr void Rol(Machine& m)
    {
        if constexpr (MODE == Mode::ACC) {
            unsigned rc = (m.a << 1) | m.carry();
            m.set<SZC>(rc);
            m.a = rc & 0xff;
        } else {
            auto adr = m.ReadEA<MODE>();
            unsigned rc = (m.Read(adr) << 1) | m.carry();
            m.Write(adr, rc);
            m.set<SZC>(rc);
        }
    }

    template <enum Reg FROM, enum Reg TO>
    static constexpr void Transfer(Machine& m)
    {
        m.Reg<TO>() = m.Reg<FROM>();
        if constexpr (TO != Reg::SP) m.set<SZ>(m.Reg<TO>());
    }

    // 65c02 opcodes

    template <int BIT>
    static constexpr void Bbr(Machine& m)
    {
        auto val = m.LoadEA<Mode::ZP>();
        auto pc = m.pc;
        int8_t diff = m.Read<POLICY::PC_AccessMode>(pc++) - 1;
        if (!(val & 1 << BIT)) {
            pc += diff;
            m.cycles++;
        }
        m.pc = pc;
    }

    template <int BIT>
    static constexpr void Bbs(Machine& m)
    {
        auto val = m.LoadEA<Mode::ZP>();
        auto pc = m.pc;
        int8_t diff = m.Read<POLICY::PC_AccessMode>(pc++) - 1;
        if (val & 1 << BIT) {
            pc += diff;
            m.cycles++;
        }
        m.pc = pc;
    }

    template <int BIT>
    static constexpr void Rmb(Machine& m)
    {
        auto adr = m.ReadEA<Mode::ZP>();
        auto val = m.Read(adr) & ~(1 << BIT);
        m.Write(adr, val);
    }

    template <int BIT>
    static constexpr void Smb(Machine& m)
    {
        auto adr = m.ReadEA<Mode::ZP>();
        auto val = m.Read(adr) | (1 << BIT);
        m.Write(adr, val);
    }

    template <enum Mode MODE>
    static constexpr void Trb(Machine& m)
    {
        auto adr = m.ReadEA<MODE>();
        auto val = m.Read(adr);
        m.set<Z>(val & m.a);
        m.Write(adr, val & (~m.a));
    }

    template <enum Mode MODE>
    static constexpr void Tsb(Machine& m)
    {
        auto adr = m.ReadEA<MODE>();
        auto val = m.Read(adr);
        m.set<Z>(val & m.a);
        m.Write(adr, val | m.a);
    }

    // ILLEGALS

    template <enum Mode MODE>
    static constexpr void Sax(Machine& m)
    {
        auto r = m.Reg<Reg::A>() & m.Reg<Reg::X>();
        m.StoreEA<MODE>(r);
    }

    template <enum Mode MODE>
    static constexpr void Lax(Machine& m)
    {
        m.a = m.x = m.LoadEA<MODE>();
        m.set<SZ>(m.a);
    }

    /////////////////////////////////////////////////////////////////////////
    ///
    ///   INSTRUCTION TABLE
    ///
    /////////////////////////////////////////////////////////////////////////

    template <bool USE_BCD = false>
    static std::vector<Instruction> makeInstructionTable(bool cpu65c02 = false)
    {
        std::vector<Instruction> instructionTable = {

            { "nop", {{ 0xea, 2, Mode::NONE, [](Machine& ) {} }} },

{ "lda", {
    { 0xa9, 2, Mode::IMM, Load<Reg::A, Mode::IMM>},
    { 0xa5, 3, Mode::ZP, Load<Reg::A, Mode::ZP>},
    { 0xb5, 4, Mode::ZPX, Load<Reg::A, Mode::ZPX>},
    { 0xad, 4, Mode::ABS, Load<Reg::A, Mode::ABS>},
    { 0xbd, 4, Mode::ABSX, Load<Reg::A, Mode::ABSX>},
    { 0xb9, 4, Mode::ABSY, Load<Reg::A, Mode::ABSY>},
    { 0xa1, 6, Mode::INDX, Load<Reg::A, Mode::INDX>},
    { 0xb1, 5, Mode::INDY, Load<Reg::A, Mode::INDY>},
} },

            { "ldx", {
                { 0xa2, 2, Mode::IMM, Load<Reg::X, Mode::IMM>},
                { 0xa6, 3, Mode::ZP, Load<Reg::X, Mode::ZP>},
                { 0xb6, 4, Mode::ZPY, Load<Reg::X, Mode::ZPY>},
                { 0xae, 4, Mode::ABS, Load<Reg::X, Mode::ABS>},
                { 0xbe, 4, Mode::ABSY, Load<Reg::X, Mode::ABSY>},
            } },

            { "ldy", {
                { 0xa0, 2, Mode::IMM, Load<Reg::Y, Mode::IMM>},
                { 0xa4, 3, Mode::ZP, Load<Reg::Y, Mode::ZP>},
                { 0xb4, 4, Mode::ZPX, Load<Reg::Y, Mode::ZPX>},
                { 0xac, 4, Mode::ABS, Load<Reg::Y, Mode::ABS>},
                { 0xbc, 4, Mode::ABSX, Load<Reg::Y, Mode::ABSX>},
            } },

            { "sta", {
                { 0x85, 3, Mode::ZP, Store<Reg::A, Mode::ZP>},
                { 0x95, 4, Mode::ZPX, Store<Reg::A, Mode::ZPX>},
                { 0x8d, 4, Mode::ABS, Store<Reg::A, Mode::ABS>},
                { 0x9d, 5, Mode::ABSX, Store<Reg::A, Mode::ABSX>},
                { 0x99, 5, Mode::ABSY, Store<Reg::A, Mode::ABSY>},
                { 0x81, 6, Mode::INDX, Store<Reg::A, Mode::INDX>},
                { 0x91, 6, Mode::INDY, Store<Reg::A, Mode::INDY>},
            } },

            { "stx", {
                { 0x86, 3, Mode::ZP, Store<Reg::X, Mode::ZP>},
                { 0x96, 4, Mode::ZPY, Store<Reg::X, Mode::ZPY>},
                { 0x8e, 4, Mode::ABS, Store<Reg::X, Mode::ABS>},
            } },

            { "sty", {
                { 0x84, 3, Mode::ZP, Store<Reg::Y, Mode::ZP>},
                { 0x94, 4, Mode::ZPX, Store<Reg::Y, Mode::ZPX>},
                { 0x8c, 4, Mode::ABS, Store<Reg::Y, Mode::ABS>},
            } },

            { "dec", {
                { 0xc6, 5, Mode::ZP, Inc<Mode::ZP, -1>},
                { 0xd6, 6, Mode::ZPX, Inc<Mode::ZPX, -1>},
                { 0xce, 6, Mode::ABS, Inc<Mode::ABS, -1>},
                { 0xde, 7, Mode::ABSX, Inc<Mode::ABSX, -1>},
            } },

            { "inc", {
                { 0xe6, 5, Mode::ZP, Inc<Mode::ZP, 1>},
                { 0xf6, 6, Mode::ZPX, Inc<Mode::ZPX, 1>},
                { 0xee, 6, Mode::ABS, Inc<Mode::ABS, 1>},
                { 0xfe, 7, Mode::ABSX, Inc<Mode::ABSX, 1>},
            } },

            { "tax", { { 0xaa, 2, Mode::NONE, Transfer<Reg::A, Reg::X> } } },
            { "txa", { { 0x8a, 2, Mode::NONE, Transfer<Reg::X, Reg::A> } } },
            { "tay", { { 0xa8, 2, Mode::NONE, Transfer<Reg::A, Reg::Y> } } },
            { "tya", { { 0x98, 2, Mode::NONE, Transfer<Reg::Y, Reg::A> } } },
            { "txs", { { 0x9a, 2, Mode::NONE, Transfer<Reg::X, Reg::SP> } } },
            { "tsx", { { 0xba, 2, Mode::NONE, Transfer<Reg::SP, Reg::X> } } },

            { "dex", { { 0xca, 2, Mode::NONE, Inc<Reg::X, -1> } } },
            { "inx", { { 0xe8, 2, Mode::NONE, Inc<Reg::X, 1> } } },
            { "dey", { { 0x88, 2, Mode::NONE, Inc<Reg::Y, -1> } } },
            { "iny", { { 0xc8, 2, Mode::NONE, Inc<Reg::Y, 1> } } },

            { "pha", { { 0x48, 3, Mode::NONE, [](Machine& m) {
                m.stack[m.sp--] = m.a;
            } } } },

            { "pla", { { 0x68, 4, Mode::NONE, [](Machine& m) {
                m.a = m.stack[++m.sp];
            } } } },

            { "php", { { 0x08, 3, Mode::NONE, [](Machine& m) {
                m.stack[m.sp--] = m.get_SR();
            } } } },

            { "plp", { { 0x28, 4, Mode::NONE, [](Machine& m) {
                m.set_SR(m.stack[++m.sp]);
            } } } },

            { "bcc", { { 0x90, 2, Mode::REL, Branch<CARRY, CLEAR> }, } },
            { "bcs", { { 0xb0, 2, Mode::REL, Branch<CARRY, SET> }, } },
            { "bne", { { 0xd0, 2, Mode::REL, Branch<ZERO, CLEAR> }, } },
            { "beq", { { 0xf0, 2, Mode::REL, Branch<ZERO, SET> }, } },
            { "bpl", { { 0x10, 2, Mode::REL, Branch<SIGN, CLEAR> }, } },
            { "bmi", { { 0x30, 2, Mode::REL, Branch<SIGN, SET> }, } },
            { "bvc", { { 0x50, 2, Mode::REL, Branch<OVER, CLEAR> }, } },
            { "bvs", { { 0x70, 2, Mode::REL, Branch<OVER, SET> }, } },
            { "adc", {
                { 0x69, 2, Mode::IMM, Adc<Mode::IMM, USE_BCD>},
                { 0x65, 3, Mode::ZP, Adc<Mode::ZP, USE_BCD>},
                { 0x75, 4, Mode::ZPX, Adc<Mode::ZPX, USE_BCD>},
                { 0x6d, 4, Mode::ABS, Adc<Mode::ABS, USE_BCD>},
                { 0x7d, 4, Mode::ABSX, Adc<Mode::ABSX, USE_BCD>},
                { 0x79, 4, Mode::ABSY, Adc<Mode::ABSY, USE_BCD>},
                { 0x61, 6, Mode::INDX, Adc<Mode::INDX, USE_BCD>},
                { 0x71, 5, Mode::INDY, Adc<Mode::INDY, USE_BCD>},
                { 0x72, 5, Mode::INDZ, Adc<Mode::INDZ, USE_BCD>},
            } },

            { "sbc", {
                { 0xe9, 2, Mode::IMM, Sbc<Mode::IMM, USE_BCD>},
                { 0xe5, 3, Mode::ZP, Sbc<Mode::ZP, USE_BCD>},
                { 0xf5, 4, Mode::ZPX, Sbc<Mode::ZPX, USE_BCD>},
                { 0xed, 4, Mode::ABS, Sbc<Mode::ABS, USE_BCD>},
                { 0xfd, 4, Mode::ABSX, Sbc<Mode::ABSX, USE_BCD>},
                { 0xf9, 4, Mode::ABSY, Sbc<Mode::ABSY, USE_BCD>},
                { 0xe1, 6, Mode::INDX, Sbc<Mode::INDX, USE_BCD>},
                { 0xf1, 5, Mode::INDY, Sbc<Mode::INDY, USE_BCD>},
            } },

            { "cmp", {
                { 0xc9, 2, Mode::IMM, Cmp<Reg::A, Mode::IMM>},
                { 0xc5, 3, Mode::ZP, Cmp<Reg::A, Mode::ZP>},
                { 0xd5, 4, Mode::ZPX, Cmp<Reg::A, Mode::ZPX>},
                { 0xcd, 4, Mode::ABS, Cmp<Reg::A, Mode::ABS>},
                { 0xdd, 4, Mode::ABSX, Cmp<Reg::A, Mode::ABSX>},
                { 0xd9, 4, Mode::ABSY, Cmp<Reg::A, Mode::ABSY>},
                { 0xc1, 6, Mode::INDX, Cmp<Reg::A, Mode::INDX>},
                { 0xd1, 5, Mode::INDY, Cmp<Reg::A, Mode::INDY>},
            } },

            { "cpx", {
                { 0xe0, 2, Mode::IMM, Cmp<Reg::X, Mode::IMM>},
                { 0xe4, 3, Mode::ZP, Cmp<Reg::X, Mode::ZP>},
                { 0xec, 4, Mode::ABS, Cmp<Reg::X, Mode::ABS>},
            } },

            { "cpy", {
                { 0xc0, 2, Mode::IMM, Cmp<Reg::Y, Mode::IMM>},
                { 0xc4, 3, Mode::ZP, Cmp<Reg::Y, Mode::ZP>},
                { 0xcc, 4, Mode::ABS, Cmp<Reg::Y, Mode::ABS>},
            } },

            { "and", {
                { 0x29, 2, Mode::IMM, And<Mode::IMM>},
                { 0x25, 3, Mode::ZP, And<Mode::ZP>},
                { 0x35, 4, Mode::ZPX, And<Mode::ZPX>},
                { 0x2d, 4, Mode::ABS, And<Mode::ABS>},
                { 0x3d, 4, Mode::ABSX, And<Mode::ABSX>},
                { 0x39, 4, Mode::ABSY, And<Mode::ABSY>},
                { 0x21, 6, Mode::INDX, And<Mode::INDX>},
                { 0x31, 5, Mode::INDY, And<Mode::INDY>},
            } },

            { "eor", {
                { 0x49, 2, Mode::IMM, Eor<Mode::IMM>},
                { 0x45, 3, Mode::ZP, Eor<Mode::ZP>},
                { 0x55, 4, Mode::ZPX, Eor<Mode::ZPX>},
                { 0x4d, 4, Mode::ABS, Eor<Mode::ABS>},
                { 0x5d, 4, Mode::ABSX, Eor<Mode::ABSX>},
                { 0x59, 4, Mode::ABSY, Eor<Mode::ABSY>},
                { 0x41, 6, Mode::INDX, Eor<Mode::INDX>},
                { 0x51, 5, Mode::INDY, Eor<Mode::INDY>},
            } },

            { "ora", {
                { 0x09, 2, Mode::IMM, Ora<Mode::IMM>},
                { 0x05, 3, Mode::ZP, Ora<Mode::ZP>},
                { 0x15, 4, Mode::ZPX, Ora<Mode::ZPX>},
                { 0x0d, 4, Mode::ABS, Ora<Mode::ABS>},
                { 0x1d, 4, Mode::ABSX, Ora<Mode::ABSX>},
                { 0x19, 4, Mode::ABSY, Ora<Mode::ABSY>},
                { 0x01, 6, Mode::INDX, Ora<Mode::INDX>},
                { 0x11, 5, Mode::INDY, Ora<Mode::INDY>},
            } },

            { "sec", { { 0x38, 2, Mode::NONE, Set<CARRY, true> } } },
            { "clc", { { 0x18, 2, Mode::NONE, Set<CARRY, false> } } },
            { "sei", { { 0x78, 2, Mode::NONE, Set<IRQ, true> } } },
            { "cli", { { 0x58, 2, Mode::NONE, Set<IRQ, false> } } },
            { "sed", { { 0xf8, 2, Mode::NONE, Set<DECIMAL, true> } } },
            { "cld", { { 0xd8, 2, Mode::NONE, Set<DECIMAL, false> } } },
            { "clv", { { 0xb8, 2, Mode::NONE, Set<OVER, false> } } },

            { "lsr", {
                { 0x4a, 2, Mode::NONE, Lsr<Mode::ACC>},
                { 0x4a, 2, Mode::ACC, Lsr<Mode::ACC>},
                { 0x46, 5, Mode::ZP, Lsr<Mode::ZP>},
                { 0x56, 6, Mode::ZPX, Lsr<Mode::ZPX>},
                { 0x4e, 6, Mode::ABS, Lsr<Mode::ABS>},
                { 0x5e, 7, Mode::ABSX, Lsr<Mode::ABSX>},
            } },

            { "asl", {
                { 0x0a, 2, Mode::NONE, Asl<Mode::ACC>},
                { 0x0a, 2, Mode::ACC, Asl<Mode::ACC>},
                { 0x06, 5, Mode::ZP, Asl<Mode::ZP>},
                { 0x16, 6, Mode::ZPX, Asl<Mode::ZPX>},
                { 0x0e, 6, Mode::ABS, Asl<Mode::ABS>},
                { 0x1e, 7, Mode::ABSX, Asl<Mode::ABSX>},
            } },

            { "ror", {
                { 0x6a, 2, Mode::NONE, Ror<Mode::ACC>},
                { 0x6a, 2, Mode::ACC, Ror<Mode::ACC>},
                { 0x66, 5, Mode::ZP, Ror<Mode::ZP>},
                { 0x76, 6, Mode::ZPX, Ror<Mode::ZPX>},
                { 0x6e, 6, Mode::ABS, Ror<Mode::ABS>},
                { 0x7e, 7, Mode::ABSX, Ror<Mode::ABSX>},
            } },

            { "rol", {
                { 0x2a, 2, Mode::NONE, Rol<Mode::ACC>},
                { 0x2a, 2, Mode::ACC, Rol<Mode::ACC>},
                { 0x26, 5, Mode::ZP, Rol<Mode::ZP>},
                { 0x36, 6, Mode::ZPX, Rol<Mode::ZPX>},
                { 0x2e, 6, Mode::ABS, Rol<Mode::ABS>},
                { 0x3e, 7, Mode::ABSX, Rol<Mode::ABSX>},
            } },

            { "bit", {
                { 0x24, 3, Mode::ZP, Bit<Mode::ZP>},
                { 0x2c, 4, Mode::ABS, Bit<Mode::ABS>},
            } },

            { "rti", {
                { 0x40, 6, Mode::NONE, [](Machine& m) {
                    m.set_SR(m.stack[m.sp+1]);
                    m.pc = (m.stack[m.sp+2] | (m.stack[m.sp+3]<<8));
                    m.sp += 3;
                } }
            } },

            { "brk", {
                { 0x00, 7, Mode::NONE, [](Machine& m) {
                    auto n = m.ReadPC();
                  if (POLICY::breakFn(m.policy(), n)) {
                      m.realCycles = m.cycles;
                      m.cycles = std::numeric_limits<decltype(m.cycles)>::max()-32;
                      return;
                  }

                    m.stack[m.sp] = m.pc >> 8;
                    m.stack[m.sp-1] = m.pc & 0xff;
                    m.stack[m.sp-2] = m.get_SR();
                    m.sp -= 3;
                    m.pc = m.Read16(m.to_adr(0xfe, 0xff));
                } },
                { 0x00, 7, Mode::IMM, [](Machine& m) {
                    auto what = m.ReadPC();
                    if (POLICY::breakFn(m.policy(), what)) {
                        m.realCycles = m.cycles;
                        m.cycles = std::numeric_limits<decltype(m.cycles)>::max()-32;
                        return;
                    }
                    m.stack[m.sp] = m.pc >> 8;
                    m.stack[m.sp-1] = m.pc & 0xff;
                    m.stack[m.sp-2] = m.get_SR();
                    m.sp -= 3;
                    m.pc = m.Read16(m.to_adr(0xfe, 0xff));
                } }
            } },

            { "rts", {
                { 0x60, 6, Mode::NONE, [](Machine& m) {
                    if constexpr (POLICY::ExitOnStackWrap) {
                        if (m.sp == 0xff) {
                            m.realCycles = m.cycles;
                            m.cycles = std::numeric_limits<decltype(m.cycles)>::max()-32;
                            return;
                        }
                    }
                    m.pc = (m.stack[m.sp+1] | (m.stack[m.sp+2]<<8))+1;
                    m.sp += 2;
                } }
            } },

            { "jmp", {
                { 0x4c, 3, Mode::ABS, [](Machine& m) {
                    m.pc = m.ReadPC16();
                } },
                { 0x6c, 5, Mode::IND, [](Machine& m) {
                    m.pc = m.Read16(m.ReadPC16());
                } }
            } },

            { "jsr", {
                { 0x20, 6, Mode::ABS, [](Machine& m) {
                    m.stack[m.sp] = (m.pc+1) >> 8;
                    m.stack[m.sp-1] = (m.pc+1) & 0xff;
                    m.sp -= 2;
                    m.pc = m.ReadPC16();
                    if ((m.pc & POLICY::jsrMask) != 0) {
                        if (POLICY::jsrFn(m.policy(), m.pc)) {
                            // jsr was overriden
                            m.pc = (m.stack[m.sp+1] | (m.stack[m.sp+2]<<8))+1;
                            m.sp += 2;
                        }
                    }
                } }
            } },
        };

        auto mergeInstructions = [&](std::vector<Instruction> const& vec) {
            for (auto const& v : vec) {
                auto it = std::find_if(
                    instructionTable.begin(), instructionTable.end(),
                    [&](auto const& i) { return strcmp(i.name, v.name) == 0; });
                if (it != instructionTable.end()) {
                    it->opcodes.insert(it->opcodes.begin(), v.opcodes.begin(),
                                       v.opcodes.end());
                } else {
                    instructionTable.push_back(v);
                }
            }
        };

        if (cpu65c02) {

            std::vector<Instruction> instructions65c02 = {
                {"adc", {{0x72, 5, Mode::INDZ, Adc<Mode::INDZ, USE_BCD>}}},
                {"and", {{0x32, 5, Mode::INDZ, And<Mode::INDZ>}}},
                {"cmp", {{0xd2, 5, Mode::INDZ, Cmp<Reg::A, Mode::INDZ>}}},
                {"eor", {{0x52, 5, Mode::INDZ, Eor<Mode::INDZ>}}},
                {"lda", {{0xb2, 5, Mode::INDZ, Load<Reg::A, Mode::INDZ>}}},
                {"ora", {{0x12, 5, Mode::INDZ, Ora<Mode::INDZ>}}},
                {"sbc", {{0xf2, 5, Mode::INDZ, Sbc<Mode::INDZ, USE_BCD>}}},
                {"sta", {{0x92, 5, Mode::INDZ, Store<Reg::A, Mode::INDZ>}}},
                {"phx",
                 {{0xda, 3, Mode::NONE,
                   [](Machine& m) { m.stack[m.sp--] = m.x; }}}},
                {"phy",
                 {{0x5a, 3, Mode::NONE,
                   [](Machine& m) { m.stack[m.sp--] = m.y; }}}},
                {"plx",
                 {{0xfa, 4, Mode::NONE,
                   [](Machine& m) { m.x = m.stack[++m.sp]; }}}},
                {"ply",
                 {{0x7a, 4, Mode::NONE,
                   [](Machine& m) { m.y = m.stack[++m.sp]; }}}},
                {"stz",
                 {{0x64, 3, Mode::ZP, Store0<Mode::ZP>},
                  {0x74, 4, Mode::ZPX, Store0<Mode::ZPX>},
                  {0x9c, 4, Mode::ABS, Store0<Mode::ABS>},
                  {0x9e, 5, Mode::ABSX, Store0<Mode::ABSX>}}},

                {"trb",
                 {{0x14, 5, Mode::ZP, Trb<Mode::ZP>},
                  {0x1c, 6, Mode::ABS, Trb<Mode::ABS>}}},
                {"tsb",
                 {{0x04, 5, Mode::ZP, Tsb<Mode::ZP>},
                  {0x0c, 6, Mode::ABS, Tsb<Mode::ABS>}}},

                {"bra", {{0x80, 2, Mode::REL, Branch}}},

                {"bbr0", {{0x0f, 5, Mode::ZP_REL, Bbr<0>}}},
                {"bbr1", {{0x1f, 5, Mode::ZP_REL, Bbr<1>}}},
                {"bbr2", {{0x2f, 5, Mode::ZP_REL, Bbr<2>}}},
                {"bbr3", {{0x3f, 5, Mode::ZP_REL, Bbr<3>}}},
                {"bbr4", {{0x4f, 5, Mode::ZP_REL, Bbr<4>}}},
                {"bbr5", {{0x5f, 5, Mode::ZP_REL, Bbr<5>}}},
                {"bbr6", {{0x6f, 5, Mode::ZP_REL, Bbr<6>}}},
                {"bbr7", {{0x7f, 5, Mode::ZP_REL, Bbr<7>}}},
                {"bbs0", {{0x8f, 5, Mode::ZP_REL, Bbs<0>}}},
                {"bbs1", {{0x9f, 5, Mode::ZP_REL, Bbs<1>}}},
                {"bbs2", {{0xaf, 5, Mode::ZP_REL, Bbs<2>}}},
                {"bbs3", {{0xbf, 5, Mode::ZP_REL, Bbs<3>}}},
                {"bbs4", {{0xcf, 5, Mode::ZP_REL, Bbs<4>}}},
                {"bbs5", {{0xdf, 5, Mode::ZP_REL, Bbs<5>}}},
                {"bbs6", {{0xef, 5, Mode::ZP_REL, Bbs<6>}}},
                {"bbs7", {{0xff, 5, Mode::ZP_REL, Bbs<7>}}},

                {"rmb0", {{0x07, 5, Mode::ZP, Rmb<0>}}},
                {"rmb1", {{0x17, 5, Mode::ZP, Rmb<1>}}},
                {"rmb2", {{0x27, 5, Mode::ZP, Rmb<2>}}},
                {"rmb3", {{0x37, 5, Mode::ZP, Rmb<3>}}},
                {"rmb4", {{0x47, 5, Mode::ZP, Rmb<4>}}},
                {"rmb5", {{0x57, 5, Mode::ZP, Rmb<5>}}},
                {"rmb6", {{0x67, 5, Mode::ZP, Rmb<6>}}},
                {"rmb7", {{0x77, 5, Mode::ZP, Rmb<7>}}},
                {"smb0", {{0x87, 5, Mode::ZP, Smb<0>}}},
                {"smb1", {{0x97, 5, Mode::ZP, Smb<1>}}},
                {"smb2", {{0xa7, 5, Mode::ZP, Smb<2>}}},
                {"smb3", {{0xb7, 5, Mode::ZP, Smb<3>}}},
                {"smb4", {{0xc7, 5, Mode::ZP, Smb<4>}}},
                {"smb5", {{0xd7, 5, Mode::ZP, Smb<5>}}},
                {"smb6", {{0xe7, 5, Mode::ZP, Smb<6>}}},
                {"smb7", {{0xf7, 5, Mode::ZP, Smb<7>}}},

            };

            mergeInstructions(instructions65c02);
        } else {

            std::vector<Instruction> instructionsIllegal = {
                {"lax",
                 {
                     {0xa7, 3, Mode::ZP, Lax<Mode::ZP>},
                     {0xb7, 4, Mode::ZPY, Lax<Mode::ZPY>},
                     {0xaf, 4, Mode::ABS, Lax<Mode::ABS>},
                     {0xbf, 4, Mode::ABSY, Lax<Mode::ABSY>},
                     {0xa3, 6, Mode::INDX, Lax<Mode::INDX>},
                     {0xb3, 5, Mode::INDY, Lax<Mode::INDY>},
                 }},

                {"sax",
                 {
                     {0x87, 3, Mode::ZP, Sax<Mode::ZP>},
                     {0x97, 4, Mode::ZPY, Sax<Mode::ZPY>},
                     {0x8f, 4, Mode::ABS, Sax<Mode::ABS>},
                     {0x83, 6, Mode::INDX, Sax<Mode::INDX>},
                 }},
                {"lxa",
                 {{0xab, 2, Mode::IMM,
                   [](Machine& m) {
                       // NOTE: Emulate as unstable ?
                       m.a &= m.LoadEA<Mode::IMM>();
                       m.x = m.a;
                   }}}},
                {"nop",
                 {
                     {0xe2, 2, Mode::IMM,
                      [](Machine& m) { m.LoadEA<Mode::IMM>(); }},
                     {0x04, 3, Mode::ZP,
                      [](Machine& m) { m.LoadEA<Mode::ZP>(); }},
                     {0x0c, 4, Mode::ABS,
                      [](Machine& m) { m.LoadEA<Mode::ABS>(); }},
                 }},
            };
            mergeInstructions(instructionsIllegal);
        }

        return instructionTable;
    }

    uint32_t run_jumptable(uint32_t toCycles = 0x010000000)
    {
        auto& p = policy();
        cycles = 0;
        // We sometimes set 'cycles' high to break out of emulation,
        // in that case 'realCycles' will contain the real value
        realCycles = 0;
        while (cycles < toCycles) {
            if (POLICY::eachOp(p)) break;
            auto code = ReadPC();
            auto& op = jumpTable[code];
            op.op(*this);
            cycles += op.cycles;
        }

        POLICY::afterRun(p);
        if (realCycles != 0) {
            cycles = realCycles;
            realCycles = 0;
        } // else
        //   return 0;

        return cycles;
    }

    uint32_t run_switch_case(uint32_t toCycles = 0x10000000)
    {
        auto& p = policy();
        cycles = 0;
        // We sometimes set 'cycles' high to break out of emulation,
        // in that case 'realCycles' will contain the real value
        realCycles = 0;
        while (cycles < toCycles) {
            if (POLICY::eachOp(p)) break;
            auto code = ReadPC();
            switch (code) {
            case 0xea:
                cycles += 2;
                break;
            case 0xa9:
                Load<Reg::A, Mode::IMM>(*this);
                cycles += 2;
                break;
            case 0xa5:
                Load<Reg::A, Mode::ZP>(*this);
                cycles += 3;
                break;
            case 0xb5:
                Load<Reg::A, Mode::ZPX>(*this);
                cycles += 4;
                break;
            case 0xad:
                Load<Reg::A, Mode::ABS>(*this);
                cycles += 4;
                break;
            case 0xbd:
                Load<Reg::A, Mode::ABSX>(*this);
                cycles += 4;
                break;
            case 0xb9:
                Load<Reg::A, Mode::ABSY>(*this);
                cycles += 4;
                break;
            case 0xa1:
                Load<Reg::A, Mode::INDX>(*this);
                cycles += 6;
                break;
            case 0xb1:
                Load<Reg::A, Mode::INDY>(*this);
                cycles += 5;
                break;
            case 0xa2:
                Load<Reg::X, Mode::IMM>(*this);
                cycles += 2;
                break;
            case 0xa6:
                Load<Reg::X, Mode::ZP>(*this);
                cycles += 3;
                break;
            case 0xb6:
                Load<Reg::X, Mode::ZPY>(*this);
                cycles += 4;
                break;
            case 0xae:
                Load<Reg::X, Mode::ABS>(*this);
                cycles += 4;
                break;
            case 0xbe:
                Load<Reg::X, Mode::ABSY>(*this);
                cycles += 4;
                break;
            case 0xa0:
                Load<Reg::Y, Mode::IMM>(*this);
                cycles += 2;
                break;
            case 0xa4:
                Load<Reg::Y, Mode::ZP>(*this);
                cycles += 3;
                break;
            case 0xb4:
                Load<Reg::Y, Mode::ZPX>(*this);
                cycles += 4;
                break;
            case 0xac:
                Load<Reg::Y, Mode::ABS>(*this);
                cycles += 4;
                break;
            case 0xbc:
                Load<Reg::Y, Mode::ABSX>(*this);
                cycles += 4;
                break;
            case 0x85:
                Store<Reg::A, Mode::ZP>(*this);
                cycles += 3;
                break;
            case 0x95:
                Store<Reg::A, Mode::ZPX>(*this);
                cycles += 4;
                break;
            case 0x8d:
                Store<Reg::A, Mode::ABS>(*this);
                cycles += 4;
                break;
            case 0x9d:
                Store<Reg::A, Mode::ABSX>(*this);
                cycles += 5;
                break;
            case 0x99:
                Store<Reg::A, Mode::ABSY>(*this);
                cycles += 5;
                break;
            case 0x81:
                Store<Reg::A, Mode::INDX>(*this);
                cycles += 6;
                break;
            case 0x91:
                Store<Reg::A, Mode::INDY>(*this);
                cycles += 6;
                break;
            case 0x86:
                Store<Reg::X, Mode::ZP>(*this);
                cycles += 3;
                break;
            case 0x96:
                Store<Reg::X, Mode::ZPY>(*this);
                cycles += 4;
                break;
            case 0x8e:
                Store<Reg::X, Mode::ABS>(*this);
                cycles += 4;
                break;
            case 0x84:
                Store<Reg::Y, Mode::ZP>(*this);
                cycles += 3;
                break;
            case 0x94:
                Store<Reg::Y, Mode::ZPX>(*this);
                cycles += 4;
                break;
            case 0x8c:
                Store<Reg::Y, Mode::ABS>(*this);
                cycles += 4;
                break;
            case 0xc6:
                Inc<Mode::ZP, -1>(*this);
                cycles += 5;
                break;
            case 0xd6:
                Inc<Mode::ZPX, -1>(*this);
                cycles += 6;
                break;
            case 0xce:
                Inc<Mode::ABS, -1>(*this);
                cycles += 6;
                break;
            case 0xde:
                Inc<Mode::ABSX, -1>(*this);
                cycles += 7;
                break;
            case 0xe6:
                Inc<Mode::ZP, 1>(*this);
                cycles += 5;
                break;
            case 0xf6:
                Inc<Mode::ZPX, 1>(*this);
                cycles += 6;
                break;
            case 0xee:
                Inc<Mode::ABS, 1>(*this);
                cycles += 6;
                break;
            case 0xfe:
                Inc<Mode::ABSX, 1>(*this);
                cycles += 7;
                break;
            case 0xaa:
                Transfer<Reg::A, Reg::X>(*this);
                cycles += 2;
                break;
            case 0x8a:
                Transfer<Reg::X, Reg::A>(*this);
                cycles += 2;
                break;
            case 0xa8:
                Transfer<Reg::A, Reg::Y>(*this);
                cycles += 2;
                break;
            case 0x98:
                Transfer<Reg::Y, Reg::A>(*this);
                cycles += 2;
                break;
            case 0x9a:
                Transfer<Reg::X, Reg::SP>(*this);
                cycles += 2;
                break;
            case 0xba:
                Transfer<Reg::SP, Reg::X>(*this);
                cycles += 2;
                break;
            case 0xca:
                Inc<Reg::X, -1>(*this);
                cycles += 2;
                break;
            case 0xe8:
                Inc<Reg::X, 1>(*this);
                cycles += 2;
                break;
            case 0x88:
                Inc<Reg::Y, -1>(*this);
                cycles += 2;
                break;
            case 0xc8:
                Inc<Reg::Y, 1>(*this);
                cycles += 2;
                break;

            case 0x48:
                stack[sp--] = a;
                cycles += 3;
                break;

            case 0x68:
                a = stack[++sp];
                cycles += 4;
                break;

            case 0x08:
                stack[sp--] = get_SR();
                cycles += 3;
                break;

            case 0x28:
                set_SR(stack[++sp]);
                cycles += 4;
                break;

            case 0x90:
                Branch<CARRY, CLEAR>(*this);
                cycles += 2;
                break;
            case 0xb0:
                Branch<CARRY, SET>(*this);
                cycles += 2;
                break;
            case 0xd0:
                Branch<ZERO, CLEAR>(*this);
                cycles += 2;
                break;
            case 0xf0:
                Branch<ZERO, SET>(*this);
                cycles += 2;
                break;
            case 0x10:
                Branch<SIGN, CLEAR>(*this);
                cycles += 2;
                break;
            case 0x30:
                Branch<SIGN, SET>(*this);
                cycles += 2;
                break;
            case 0x50:
                Branch<OVER, CLEAR>(*this);
                cycles += 2;
                break;
            case 0x70:
                Branch<OVER, SET>(*this);
                cycles += 2;
                break;
            case 0x69:
                decMode ? Adc<Mode::IMM, true>(*this) : Adc<Mode::IMM, false>(*this) ;
                cycles += 2;
                break;
            case 0x65:
                decMode ? Adc<Mode::ZP, true>(*this) : Adc<Mode::ZP, false>(*this) ;
                cycles += 3;
                break;
            case 0x75:
                decMode ? Adc<Mode::ZPX, true>(*this) : Adc<Mode::ZPX, false>(*this) ;
                cycles += 4;
                break;
            case 0x6d:
                decMode ? Adc<Mode::ABS, true>(*this) : Adc<Mode::ABS, false>(*this) ;
                cycles += 4;
                break;
            case 0x7d:
                decMode ? Adc<Mode::ABSX, true>(*this) : Adc<Mode::ABSX, false>(*this) ;
                cycles += 4;
                break;
            case 0x79:
                decMode ? Adc<Mode::ABSY, true>(*this) : Adc<Mode::ABSY, false>(*this) ;
                cycles += 4;
                break;
            case 0x61:
                decMode ? Adc<Mode::INDX, true>(*this) : Adc<Mode::INDX, false>(*this) ;
                cycles += 6;
                break;
            case 0x71:
                decMode ? Adc<Mode::INDY, true>(*this) : Adc<Mode::INDY, false>(*this) ;
                cycles += 5;
                break;
            case 0x72:
                decMode ? Adc<Mode::INDZ, true>(*this) : Adc<Mode::INDZ, false>(*this) ;
                cycles += 5;
                break;

            case 0xe9:
                decMode ? Sbc<Mode::IMM, true>(*this) : Sbc<Mode::IMM, false>(*this) ;
                cycles += 2;
                break;
            case 0xe5:
                decMode ? Sbc<Mode::ZP, true>(*this) : Sbc<Mode::ZP, false>(*this) ;
                cycles += 3;
                break;
            case 0xf5:
                decMode ? Sbc<Mode::ZPX, true>(*this) : Sbc<Mode::ZPX, false>(*this) ;
                cycles += 4;
                break;
            case 0xed:
                decMode ? Sbc<Mode::ABS, true>(*this) : Sbc<Mode::ABS, false>(*this) ;
                cycles += 4;
                break;
            case 0xfd:
                decMode ? Sbc<Mode::ABSX, true>(*this) : Sbc<Mode::ABSX, false>(*this) ;
                cycles += 4;
                break;
            case 0xf9:
                decMode ? Sbc<Mode::ABSY, true>(*this) : Sbc<Mode::ABSY, false>(*this) ;
                cycles += 4;
                break;
            case 0xe1:
                decMode ? Sbc<Mode::INDX, true>(*this) : Sbc<Mode::INDX, false>(*this) ;
                cycles += 6;
                break;
            case 0xf1:
                decMode ? Sbc<Mode::INDY, true>(*this) : Sbc<Mode::INDY, false>(*this) ;
                cycles += 5;
                break;

            case 0xc9:
                Cmp<Reg::A, Mode::IMM>(*this);
                cycles += 2;
                break;
            case 0xc5:
                Cmp<Reg::A, Mode::ZP>(*this);
                cycles += 3;
                break;
            case 0xd5:
                Cmp<Reg::A, Mode::ZPX>(*this);
                cycles += 4;
                break;
            case 0xcd:
                Cmp<Reg::A, Mode::ABS>(*this);
                cycles += 4;
                break;
            case 0xdd:
                Cmp<Reg::A, Mode::ABSX>(*this);
                cycles += 4;
                break;
            case 0xd9:
                Cmp<Reg::A, Mode::ABSY>(*this);
                cycles += 4;
                break;
            case 0xc1:
                Cmp<Reg::A, Mode::INDX>(*this);
                cycles += 6;
                break;
            case 0xd1:
                Cmp<Reg::A, Mode::INDY>(*this);
                cycles += 5;
                break;
            case 0xe0:
                Cmp<Reg::X, Mode::IMM>(*this);
                cycles += 2;
                break;
            case 0xe4:
                Cmp<Reg::X, Mode::ZP>(*this);
                cycles += 3;
                break;
            case 0xec:
                Cmp<Reg::X, Mode::ABS>(*this);
                cycles += 4;
                break;

            case 0xc0:
                Cmp<Reg::Y, Mode::IMM>(*this);
                cycles += 2;
                break;
            case 0xc4:
                Cmp<Reg::Y, Mode::ZP>(*this);
                cycles += 3;
                break;
            case 0xcc:
                Cmp<Reg::Y, Mode::ABS>(*this);
                cycles += 4;
                break;

            case 0x29:
                And<Mode::IMM>(*this);
                cycles += 2;
                break;
            case 0x25:
                And<Mode::ZP>(*this);
                cycles += 3;
                break;
            case 0x35:
                And<Mode::ZPX>(*this);
                cycles += 4;
                break;
            case 0x2d:
                And<Mode::ABS>(*this);
                cycles += 4;
                break;
            case 0x3d:
                And<Mode::ABSX>(*this);
                cycles += 4;
                break;
            case 0x39:
                And<Mode::ABSY>(*this);
                cycles += 4;
                break;
            case 0x21:
                And<Mode::INDX>(*this);
                cycles += 6;
                break;
            case 0x31:
                And<Mode::INDY>(*this);
                cycles += 5;
                break;

            case 0x49:
                Eor<Mode::IMM>(*this);
                cycles += 2;
                break;
            case 0x45:
                Eor<Mode::ZP>(*this);
                cycles += 3;
                break;
            case 0x55:
                Eor<Mode::ZPX>(*this);
                cycles += 4;
                break;
            case 0x4d:
                Eor<Mode::ABS>(*this);
                cycles += 4;
                break;
            case 0x5d:
                Eor<Mode::ABSX>(*this);
                cycles += 4;
                break;
            case 0x59:
                Eor<Mode::ABSY>(*this);
                cycles += 4;
                break;
            case 0x41:
                Eor<Mode::INDX>(*this);
                cycles += 6;
                break;
            case 0x51:
                Eor<Mode::INDY>(*this);
                cycles += 5;
                break;

            case 0x09:
                Ora<Mode::IMM>(*this);
                cycles += 2;
                break;
            case 0x05:
                Ora<Mode::ZP>(*this);
                cycles += 3;
                break;
            case 0x15:
                Ora<Mode::ZPX>(*this);
                cycles += 4;
                break;
            case 0x0d:
                Ora<Mode::ABS>(*this);
                cycles += 4;
                break;
            case 0x1d:
                Ora<Mode::ABSX>(*this);
                cycles += 4;
                break;
            case 0x19:
                Ora<Mode::ABSY>(*this);
                cycles += 4;
                break;
            case 0x01:
                Ora<Mode::INDX>(*this);
                cycles += 6;
                break;
            case 0x11:
                Ora<Mode::INDY>(*this);
                cycles += 5;
                break;

            case 0x38:
                Set<CARRY, true>(*this);
                cycles += 2;
                break;
            case 0x18:
                Set<CARRY, false>(*this);
                cycles += 2;
                break;
            case 0x78:
                Set<IRQ, true>(*this);
                cycles += 2;
                break;
            case 0x58:
                Set<IRQ, false>(*this);
                cycles += 2;
                break;
            case 0xf8:
                Set<DECIMAL, true>(*this);
                cycles += 2;
                break;
            case 0xd8:
                Set<DECIMAL, false>(*this);
                cycles += 2;
                break;
            case 0xb8:
                Set<OVER, false>(*this);
                cycles += 2;
                break;

            case 0x4a:
                Lsr<Mode::ACC>(*this);
                cycles += 2;
                break;
            case 0x46:
                Lsr<Mode::ZP>(*this);
                cycles += 5;
                break;
            case 0x56:
                Lsr<Mode::ZPX>(*this);
                cycles += 6;
                break;
            case 0x4e:
                Lsr<Mode::ABS>(*this);
                cycles += 6;
                break;
            case 0x5e:
                Lsr<Mode::ABSX>(*this);
                cycles += 7;
                break;

            case 0x0a:
                Asl<Mode::ACC>(*this);
                cycles += 2;
                break;
            case 0x06:
                Asl<Mode::ZP>(*this);
                cycles += 5;
                break;
            case 0x16:
                Asl<Mode::ZPX>(*this);
                cycles += 6;
                break;
            case 0x0e:
                Asl<Mode::ABS>(*this);
                cycles += 6;
                break;
            case 0x1e:
                Asl<Mode::ABSX>(*this);
                cycles += 7;
                break;

            case 0x6a:
                Ror<Mode::ACC>(*this);
                cycles += 2;
                break;
            case 0x66:
                Ror<Mode::ZP>(*this);
                cycles += 5;
                break;
            case 0x76:
                Ror<Mode::ZPX>(*this);
                cycles += 6;
                break;
            case 0x6e:
                Ror<Mode::ABS>(*this);
                cycles += 6;
                break;
            case 0x7e:
                Ror<Mode::ABSX>(*this);
                cycles += 7;
                break;

            case 0x2a:
                Rol<Mode::ACC>(*this);
                cycles += 2;
                break;
            case 0x26:
                Rol<Mode::ZP>(*this);
                cycles += 5;
                break;
            case 0x36:
                Rol<Mode::ZPX>(*this);
                cycles += 6;
                break;
            case 0x2e:
                Rol<Mode::ABS>(*this);
                cycles += 6;
                break;
            case 0x3e:
                Rol<Mode::ABSX>(*this);
                cycles += 7;
                break;

            case 0x24:
                Bit<Mode::ZP>(*this);
                cycles += 3;
                break;
            case 0x2c:
                Bit<Mode::ABS>(*this);
                cycles += 4;
                break;

            case 0x40:
                set_SR(stack[sp + 1]);
                pc = (stack[sp + 2] | (stack[sp + 3] << 8));
                sp += 3;
                cycles += 6;
                break;
            case 0x00: {
                auto n = ReadPC();
                if (POLICY::breakFn(policy(), n)) {
                    realCycles = cycles;
                    cycles = std::numeric_limits<decltype(cycles)>::max() - 32;
                    break;
                }
                stack[sp] = pc >> 8;
                stack[sp - 1] = pc & 0xff;
                stack[sp - 2] = get_SR();
                sp -= 3;
                pc = Read16(to_adr(0xfe, 0xff));
                cycles += 7;
                break;
            }
            case 0x60:
                if constexpr (POLICY::ExitOnStackWrap) {
                    if (sp == 0xff) {
                        realCycles = cycles;
                        cycles =
                            std::numeric_limits<decltype(cycles)>::max() - 32;
                        break;
                    }
                }
                pc = (stack[sp + 1] | (stack[sp + 2] << 8)) + 1;
                sp += 2;
                cycles += 6;
                break;
            case 0x4c:
                pc = ReadPC16();
                cycles += 3;
                break;
            case 0x6c:
                pc = Read16(ReadPC16());
                cycles += 5;
                break;
            case 0x20:
                stack[sp] = (pc + 1) >> 8;
                stack[sp - 1] = (pc + 1) & 0xff;
                sp -= 2;
                pc = ReadPC16();
                if ((pc & POLICY::jsrMask) != 0) {
                    if (POLICY::jsrFn(policy(), pc)) {
                        // jsr was overriden
                        pc = (stack[sp + 1] | (stack[sp + 2] << 8)) + 1;
                        sp += 2;
                    }
                }
                cycles += 6;
                break;
            // Illegal opcodes
            case 0xa7: Lax<Mode::ZP>(*this); cycles += 3; break;
            case 0xb7: Lax<Mode::ZPY>(*this); cycles += 4; break;
            case 0xaf: Lax<Mode::ABS>(*this); cycles += 4; break;
            case 0xbf: Lax<Mode::ABSY>(*this); cycles += 4; break;
            case 0xa3: Lax<Mode::INDX>(*this); cycles += 6; break;
            case 0xb3: Lax<Mode::INDY>(*this); cycles += 5; break;
            case 0x87: Sax<Mode::ZP>(*this); cycles += 3; break;
            case 0x97: Sax<Mode::ZPY>(*this); cycles += 4; break;
            case 0x8f: Sax<Mode::ABS>(*this); cycles += 4; break;
            case 0x83: Sax<Mode::INDX>(*this); cycles += 6; break;
            case 0xab:
                  // NOTE: Emulate as unstable ?
                  a &= LoadEA<Mode::IMM>();
                  x = a;
                  cycles += 2;
                  break;
            case 0xe2: LoadEA<Mode::IMM>(); cycles += 2; break;
            case 0x04: LoadEA<Mode::ZP>(); cycles += 2; break;
            case 0x0c: LoadEA<Mode::ABS>(); cycles += 2; break;
            default:
                printf("ILLEGAL OPCODE %x at %04x\n", code, pc);
                break;
            }
        }
        POLICY::afterRun(p);
        if (realCycles != 0) {
            cycles = realCycles;
            realCycles = 0;
        }
        return cycles;
    }
};
} // namespace sixfive
