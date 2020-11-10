#define CATCH_CONFIG_MAIN
#include "../catch.hpp"

#include "ansiconsole.h"
#include "localterminal.h"
#include "newconsole.h"

TEST_CASE("ansi", "")
{
    //using namespace bbs;
    //auto terminal = std::make_shared<LocalTerminal>();
    //auto console = std::make_shared<AnsiConsole>(*terminal.get());
    //REQUIRE(console.get() != nullptr);
    //console->put(0, 0, "Testing", 0xffffff, 0x0000ff);


    conio::AnsiConsole con;
    con.clear();
    con.put({10, 10}, {0xffffff, 0x0000ff}, "Hello");
    con.put({5, 5}, {0xffffff, 0x0000ff}, "Hoho");
    con.flush();

}
