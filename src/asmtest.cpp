

#include "catch.hpp"

#include "assembler.h"
#include "png.h"
#include "test_utils.h"

#include <coreutils/crc.h>

#include "machine.h"
#include <cmath>
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
        if (checkFile(e)) {
            it = errs.erase(it);
        } else {
            it++;
        }
    }
    return errs.empty();
}

TEST_CASE("png.remap", "[assembler]")
{
    Image image = loadPng((projDir() / "data" / "tiles.png").string());

    int i = 0;
    for_all_pixels(image.pixels, image.bpp, [&](uint8_t& c) {
        // c = ~c;
    });

    savePng("test.png", image);

    image = loadPng((projDir() / "data" / "mountain.png").string());

    remap_image(image, {0x0, 0x444444, 0x888888, 0xccccc, 0xffffff});

    savePng("remapped.png", image);
}

TEST_CASE("png.layout", "[assembler]")
{
    auto get = [&](auto const& vec, int n) -> uint16_t {
        return vec[n * 2] | (vec[n * 2 + 1] << 8);
    };

    Image image = loadPng((projDir() / "data" / "test.png").string());

    REQUIRE(get(image.colors, 0) == 0);
    REQUIRE(get(image.colors, 1) == 0x000a);
    REQUIRE(get(image.colors, 2) == 0x00f0);

    auto pixels = std::any_cast<std::vector<uint8_t>>(image.pixels);
    auto tiles = layoutTiles(pixels, 32, 8, 8, 0);

    LOGI("TILES %d", tiles.size());
    for (int i = 0; i < 8 * 8; i++) {
        fmt::print("{:02x} ", tiles[i]);
    }
    puts("");
    for (int i = 0; i < 8 * 8; i++) {
        fmt::print("{:02x} ", tiles[i + 8 * 8]);
    }

    pixels = tiles;
    auto indexes = indexTiles(tiles, 8 * 8);

    REQUIRE(get(indexes, 0) == get(indexes, 15));
    REQUIRE(get(indexes, 8) == get(indexes, 10));
    REQUIRE(get(indexes, 2) == get(indexes, 13));
    REQUIRE(tiles.size() == 8 * 8 * 8);

    auto size = 8 * 8;
    for (int i = 0; i < 16; i++) {

        auto* ptr0 = &pixels[i * size];
        auto index = get(indexes, i);

        auto* ptr1 = &tiles[index * size];
        auto crc0 = crc32(reinterpret_cast<const uint32_t*>(ptr0), size / 4);
        auto crc1 = crc32(reinterpret_cast<const uint32_t*>(ptr1), size / 4);

        REQUIRE(crc0 == crc1);
    }
}

TEST_CASE("any_callable", "[assembler]")
{
    AnyCallable fn;
    fn = [](std::string s) -> long { return std::stol(s) + 3; };

    auto res = fn({std::any("100"s)});
    REQUIRE(std::any_cast<double>(res) == 103);
}

TEST_CASE("png", "[assembler]")
{

    Image image = loadPng((projDir() / "data" / "tiles.png").string());
    auto tiles = layoutTiles(image.pixels, 96, 2, 16, 0);

    uint8_t* tile8 = &tiles[8 * 2 * 16];

    for (int i = 0; i < 256; i += 2) {
        if (i % 32 == 0) puts("--------");
        fmt::print("{:02x} {:02x}\n", tiles[i], tiles[i + 1]);
    }

    LOGI("%x %x %x", tile8[0], tile8[1], tiles[2]);

    REQUIRE(tile8[0] == 0b01011111);
    REQUIRE(tile8[1] == 0b11111010);

    auto tiles8 = layoutTiles(tiles, 2, 1, 8, 0);
}

TEST_CASE("all", "[assembler]")
{
    for (auto const& p : fs::directory_iterator(projDir() / "tests")) {
        Assembler ass;
        fmt::print(fmt::fg(fmt::color::yellow), "{}\n", p.path().string());
        ass.parse_path(p.path());
        for (auto const& e : ass.getErrors()) {
            fmt::print(fmt::fg(fmt::color::coral), "ERROR '{}' in {}:{}\n",
                       e.message, e.file, e.line);
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
    int sine_table = static_cast<int>(syms.get<Number>("sine_table"));
    for (int i = 0; i < 256; i++) {
        unsigned v = static_cast<int>(sin(i * M_PI / 180) * 127);
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
        v.reserve(10);
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
