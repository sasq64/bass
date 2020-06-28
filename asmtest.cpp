

#include "catch.hpp"

#include "assembler.h"

#include "machine.h"
#include <coreutils/log.h>
#include <fmt/format.h>
#include <math.h>
#include <string>

using namespace std::string_literals;

void printSymbols(Assembler& ass)
{
    using std::any_cast;
    ass.getSymbols().forAll([](std::string const& name, std::any const& val) {
        if (auto const* v = any_cast<Number>(&val)) {
            fmt::print("{} == 0x{:x}\n", name, (int)*v);
        } else if (auto const* v = any_cast<std::vector<uint8_t>>(&val)) {
            fmt::print("{} == [{} bytes]\n", name, v->size());
        } else if (auto const* v = any_cast<std::string>(&val)) {
            fmt::print("{} == \"{}\"\n", name, *v);
        } else {
            fmt::print("{} == ?{}\n", name, val.type().name());
        }
    });
}

struct Tester
{
    std::shared_ptr<Assembler> a;
    Tester() : a{std::make_shared<Assembler>()} {};

    Tester(std::string const& code) : a{std::make_shared<Assembler>()}
    {
        a->parse(code);
    };

    Tester& operator=(std::string const& code)
    {
        a = std::make_shared<Assembler>();
        a->parse(code);
        return *this;
    }

    auto& compile(std::string const& code)
    {
        a->parse(code);
        return *this;
    }

    bool noErrors() { return a->getErrors().empty(); }

    size_t mainSize() { return a->getMachine().getSection("main").data.size(); }

    template <typename T,
              typename S = std::enable_if_t<std::is_arithmetic_v<T>>>
    bool haveSymbol(std::string const& name, T v)
    {
        auto x = a->getSymbols().get<Number>(name);
        return x == v;
    }

    bool haveSymbol(std::string const& name, std::string const& v)
    {
        auto x = a->getSymbols().get<std::string_view>(name);
        return x == v;
    }
};

TEST_CASE("assembler.first", "[assembler]")
{
    Tester t;

    t = " asl a";
    REQUIRE(t.noErrors());

    t = "!rept 4 { nop }";
    REQUIRE(t.noErrors());
    REQUIRE(t.mainSize() == 4);

    t = "!rept 4 { !rept 4 { nop } }";
    REQUIRE(t.noErrors());
    REQUIRE(t.mainSize() == 16);


    logging::setLevel(logging::Level::Debug);
    t = "!enum { A = 1\nB\n C = \"xx\" }";


    for(auto err : t.a->getErrors()) {
        LOGI("%d : %s", err.line, err.message);
    }

    REQUIRE(t.noErrors());
    REQUIRE(t.mainSize() == 0);
    REQUIRE(t.haveSymbol("B", 2));
    REQUIRE(t.haveSymbol("C", "xx"));

}

TEST_CASE("assembler.basic", "[assembler]")
{
    Assembler ass;
    auto& syms = ass.getSymbols();

    ass.parse(R"(
    !org $800
    nop
label_here  nop
other_label
  nop
  third_label: rts
    nop


loop:
    bcc .skip
    lda #45
    beq loop
.skip
    !byte 1,2,3
    rts
)");

    LOGI("Parsing done");
    REQUIRE(std::any_cast<Number>(ass.evaluateExpression("loop+9")) ==
            0x805 + 9);
    REQUIRE(ass.evaluateDefinition("test(a, b)").name == "test");

    REQUIRE(syms.at<Number>("label_here") == 0x801);
    REQUIRE(syms.at<Number>("other_label") == 0x802);
    REQUIRE(syms.at<Number>("third_label") == 0x803);
    REQUIRE(syms.at<Number>("loop") == 0x805);
    REQUIRE(syms.at<Number>("loop.skip") == 0x80b);
}

TEST_CASE("assembler.syntax", "[assembler]")
{
    Assembler ass;
    ass.parse(R"(
; this is a COMMENT
    rts
; end
    !ifdef NO {
        nop
    } else {
        lda #45
    }
    )");
}

TEST_CASE("assembler.section_move", "[assembler]")
{
    Assembler ass;
    auto& syms = ass.getSymbols();
    auto& mach = ass.getMachine();

    ass.parse(R"(

    !section "utils", section.main.end
print:
    bcc .skip
    lda #45
    jsr $ff00
.skip
    rts

    !section "main", 0

    nop
    nop
    jsr print
    rts
)");

    // The bcc should jump forward 5 bytes
    REQUIRE((int)mach.getSection("utils").data[1] == 0x5);

    REQUIRE(syms.at<Number>("print.skip") == 0xd);
}

TEST_CASE("assembler.block", "[assembler]")
{
    using std::any_cast;
    Assembler ass;
    ass.parse(R"(
    !section "main", 0
    lda #4
    !rept 4 {
        nop
    }
    sta $3
    !if 2 {
        !rept 2 {
            rts
        }
    }
    rts
)");

    auto& mach = ass.getMachine();
    REQUIRE((int)mach.getSection("main").data.size() == 11);
}

TEST_CASE("assembler.sine_table", "[assembler]")
{
    using std::any_cast;
    Assembler ass;
    // logging::setLevel(logging::Level::Debug);
    ass.parse(R"(
startValue = $32
endValue = $84	; $7b
sinLength = 128
amplitude = round(endValue-startValue)

    data:
        !rept sinLength { !byte (sin((i/(sinLength-1))* Math.Pi * 2) * 0.5 + 0.5) * amplitude + startValue }
)");

    for (auto& e : ass.getErrors()) {
        fmt::print("{} in {}:{}\n", e.message, e.line, e.column);
    }
    auto& mach = ass.getMachine();
    auto const& data = mach.getSection("main").data;
    for (auto x : data) {
        fmt::print("{:0x} ", x);
    }
}

TEST_CASE("assembler.macro", "[assembler]")
{
    using std::any_cast;
    Assembler ass;
    // logging::setLevel(logging::Level::Debug);
    ass.parse(R"(

    !section "x", 0
    y = 99

!macro apa(x,y) {
    ldx #x
    !rept 5 { nop }
    !if (x ==3) {
        sta $d020
    }
    ldy #y
}

!macro test(a) {
    lda #a
    jsr other
}
    !section "main" , $800
    apa(3,4)
    test(3)
    rts
other:
    rts
)");

    auto& mach = ass.getMachine();
    auto const& data = mach.getSection("main").data;

    REQUIRE((int)data[2] == 0xea);
    REQUIRE((int)data[6] == 0xea);
    REQUIRE((int)data[8] == 0x20);
    REQUIRE((int)data[9] == 0xd0);

    auto& syms = ass.getSymbols();
    REQUIRE(syms.at<Number>("y") == 99);
}

TEST_CASE("assembler.if_macro", "[assembler]")
{
    using std::any_cast;
    Assembler ass;
    ass.parse(R"(
    !macro SetVAdr(adr) {
        lda #(adr & $ff)
        sta $9f20
        !if ((adr & 0xff) != (adr>>8)) {
            lda #(adr>>8)
        }
        sta $9f21
    }
    !org $800

    nop
    SetVAdr(0x1212)
    SetVAdr(0x3456)
    rts

)");
}

TEST_CASE("assembler.math", "[assembler]")
{
    Assembler ass;
    auto& syms = ass.getSymbols();
    ass.parse(R"(
    !section "main", $800

    lda #sin(Math.Pi)*100
    lda #cos(Math.Pi)*100
    rts

sine_table:
    !rept 256 {
        !byte sin(i*Math.Pi/180)*127
    }

)");

    auto& mach = ass.getMachine();
    int sine_table = (int)syms.get<Number>("sine_table");
    for (int i = 0; i < 256; i++) {
        unsigned v = (int)(sin(i * M_PI / 180) * 127);
        REQUIRE(mach.getSection("main").data[sine_table + i - 0x800] ==
                (v & 0xff));
    }
}

TEST_CASE("assembler.test", "[assembler]")
{
    Assembler ass;
    ass.parse(R"(
    !section "test", $2000
    !org $800
start:
    ldx #0
.loop
    lda $1000,x
    sta $2000,x
    inx
    cpx #12
    bne .loop
    rts
    !section "data", $1000
    !byte 1,2,3,4,5,6,7

!test my_test {
    jsr start
    jmp xxx
    nop
xxx:
    lda #4
}

!assert compare(tests.my_test.ram[0x2000:0x2007], bytes(1,2,3,4,5,6,7))

!assert tests.my_test.A == 4
!assert tests.my_test.ram[0x2006] == 7
!assert tests.my_test.cycles < 200

)");

    auto& mach = ass.getMachine();
    REQUIRE(mach.readRam(0x2000) == 1);
    REQUIRE(mach.readRam(0x2001) == 2);
}

TEST_CASE("assembler.functions", "[assembler]")
{
    Assembler ass;

    ass.registerFunction("add", [](uint32_t a, uint16_t b) { return a + b; });

    ass.registerFunction("table", []() {
        std::vector<uint8_t> v;
        for (int i = 0; i < 10; i++) {
            v.push_back(i * 33);
        }
        return v;
    });

    ass.registerFunction("element", [](std::vector<uint8_t> const& v,
                                       uint8_t i) { return v[i]; });

    ass.parse(R"(
    !section "main", $800
    x = 8
    data = table()
    lda #add(x,7)
    ldx #element(data,2)
    )");

    REQUIRE(ass.getMachine().getSection("main").data[1] == 0xf);
    REQUIRE(ass.getMachine().getSection("main").data[3] == 66);
}

TEST_CASE("assembler.errors", "[assembler]")
{
    Assembler ass;

    std::vector<std::pair<std::string, int>> sources = {
        {R"(!org $800
            ldz #3
             rts)",
         2},
        //
        {R"(!org $800
        !macro test(x) {
            nop
            stq x
        }
        test(2)
        rts)",
         6},
        //
        {R"(!org $800
        nop
        !if 0 {
            stq 0
        }
        !if 1 {
            nop
            stq 0
        }
        rts)",
         6},
        //
        {R"(!org $800
        lda #15
        ldx #a
        ldy #b
a       !byte 3
        rts)",
         4},
        //
        {R"(!org $800
        !section
        rts)",
         2},
        //
        {R"(!org $800
        !macro beta(z,y) {
            nop
            .if z == y {
                nop
                }
        }
        beta(1,2)
        rts)",
         7},
    };

    for (auto const& s : sources) {
        ass.parse(s.first, "test.asm");
        LOGI("Code:%s", s.first);
        auto const& errs = ass.getErrors();
        REQUIRE(!errs.empty());
        bool ok = false;
        for (auto& e : errs) {
            fmt::print("{} in {}:{}\n", e.message, e.line, e.column);
            if(e.line == (size_t)s.second)
                ok = true;
        }
        REQUIRE(ok);
    }
}

TEST_CASE("assembler.special_labels", "[assembler]")
{
    Assembler ass;
    ass.parse(R"(
    !section "a", $1000
    nop
$   lda $d012
    cmp #30
    bne -
    lda $c000
    beq +
    jmp -
$   rts
    nop

    !section "b", $1000
    nop
loop:
    lda $d012
    cmp #30
    bne loop
    lda $c000
    beq skip
    jmp loop
skip
    rts
$   nop

    !section "c", $1000
    !macro dummy(a) {
.x   lda $d012
    cmp #30
    bne .x
    lda $c000
    beq .y
    jmp .x
.y  rts
}
$   nop
    dummy(0)
$   nop

)");

    auto& mach = ass.getMachine();
    REQUIRE(mach.getSection("a").data == mach.getSection("b").data);
    REQUIRE(mach.getSection("b").data == mach.getSection("c").data);
}

