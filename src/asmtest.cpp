

#include "catch.hpp"

#include "assembler.h"
#include "test_utils.h"

#include "machine.h"
#include <cmath>
#include <coreutils/log.h>
#include <fmt/color.h>
#include <fmt/format.h>
#include <string>

using namespace std::string_literals;

void printSymbols(Assembler& ass)
{
    using std::any_cast;
    ass.getSymbols().forAll([](std::string const& name, std::any const& val) {
        if (auto const* n = any_cast<Number>(&val)) {
            fmt::print("{} == 0x{:x}\n", name, static_cast<int>(*n));
        } else if (auto const* v = any_cast<std::vector<uint8_t>>(&val)) {
            fmt::print("{} == [{} bytes]\n", name, v->size());
        } else if (auto const* s = any_cast<std::string>(&val)) {
            fmt::print("{} == \"{}\"\n", name, *s);
        } else {
            fmt::print("{} == ?{}\n", name, val.type().name());
        }
    });
}

bool checkFile(Error const& e)
{
    constexpr std::string_view errorTag = "!error";
    utils::File f{e.file};
    size_t l = 1;
    for (auto const& line : f.lines()) {
        l++;
        auto pos = line.find(errorTag);
        if (pos != std::string::npos) {
            pos += errorTag.size();
            while (line[pos] == ' ')
                pos++;
            auto match = line.substr(pos);
            if (e.line == l && e.message.find(match) != std::string::npos) {
                return true;
            }
        }
    }
    return false;
}

bool checkErrors(std::vector<Error> errs)
{
    auto it = errs.begin();
    while (it != errs.end()) {
        auto const& e = *it;
        if(checkFile(e)) {
            it = errs.erase(it);
        } else {
            it++;
        }
    }
    return errs.empty();
}

TEST_CASE("all", "[assembler]")
{
    for (auto const& p : utils::listFiles(projDir() / "tests")) {
        Assembler ass;
        fmt::print(fmt::fg(fmt::color::yellow), "{}\n", p.string());
        ass.parse_path(p);
        for (auto const& e : ass.getErrors()) {
            fmt::print(fmt::fg(fmt::color::coral), "ERROR '{}' in {}:{}\n", e.message, e.file, e.line);
        }
        if (!checkErrors(ass.getErrors())) {
            FAIL("Did not find expected errors");
        }
    }
}

TEST_CASE("assembler.sections", "[assembler]")
{
    Assembler ass;
    auto& syms = ass.getSymbols();

    ass.parse(R"(
        !section "BASIC",start=$0801,size=$d000

        !section "code",in="BASIC"
        !section "text",in="BASIC"

        !section "start",in="code" {
        start:
            !section "x",in="text" {
            hello:
                !text "hello peope"
            }
            nop
            lda hello,x
            rts
        }
    )");
    auto const& errs = ass.getErrors();
    for (auto const& e : errs) {
        fmt::print("{} in {}:{}\n", e.message, e.line, e.column);
    }
    REQUIRE(errs.empty());
    REQUIRE(syms.get<Number>("hello") == 0x0801 + 5);
    ass.getMachine().write("_test.prg", OutFmt::Prg);
    ass.getMachine().write("_test.crt", OutFmt::EasyFlash);
}

TEST_CASE("assembler.sections2", "[assembler]")
{
    Assembler ass;

    ass.parse(R"(
        !section "BASIC",start=$0801,size=$d000 {

            !section in="BASIC" {
                start: rts
            }
            !section "text",in="BASIC" {
                data: .text "hello"
            }
        }
    )");
    auto const& errs = ass.getErrors();
    for (auto const& e : errs) {
        fmt::print("{} in {}:{}\n", e.message, e.line, e.column);
    }
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
    auto const& data = mach.getSection("default").data;
    for (auto x : data) {
        fmt::print("{:0x} ", x);
    }
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
