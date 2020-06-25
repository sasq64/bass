#include "src/assembler.h"
#include "src/defines.h"
#include "src/machine.h"

#include <cstdint>
#include <cstdio>
#include <string>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {

    std::string data{(char*)Data, Size};
    Assembler ass;

    ass.parse(data, "fuzz.asm");
    puts(ass.getErrors().empty() ? "OK" : "ERROR");
    return 0;
}
