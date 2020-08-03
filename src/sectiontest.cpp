
#include "catch.hpp"

#include "machine.h"

#include <string>

using namespace std::string_literals;

#if 0
TEST_CASE("sections.basic", "[sections]")
{
    Machine m;

    auto& main = m.addSection("main");

    main.start = 0x1000;
    main.size = 0x3000;
    main.flags |= (FixedSize|FixedStart);

    auto& text = m.addSection("text", "main");
    auto& extra = m.addSection("extra", "main");
    auto& utils = m.addSection("utils", "main");

    utils.size = 0x100;
    utils.data = std::vector<uint8_t>(0x100);
    text.size = 0x200;
    text.flags |= FixedSize;

    m.layoutSections();

    REQUIRE(utils.start == 0x1000 + 0x200);
    REQUIRE(text.start == 0x1000);

    auto& a = m.addSection("a", "extra");
    auto& b = m.addSection("b", "extra");

    a.data = std::vector<uint8_t>(0x20);
    b.data = std::vector<uint8_t>(0x30);

    m.layoutSections();
    REQUIRE(text.start == 0x1000);
    REQUIRE(utils.start == 0x1000 + 0x200 + 0x50);
    REQUIRE(b.start == 0x1000 + 0x200 + 0x20);

    m.layoutSections();
    REQUIRE(utils.start == 0x1000 + 0x200 + 0x50);
    REQUIRE(b.start == 0x1000 + 0x200 + 0x20);
}
#endif
