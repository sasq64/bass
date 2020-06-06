#pragma once

#include <cstdint>

namespace sixfive {


// Adressing modes
enum AdressingMode : uint8_t
{
    BAD,
    NONE,
    ACC,
    IMM,
    REL,
    ZP,
    ZPX,
    ZPY,
    INDX,
    INDY,
    IND,
    ABS,
    ABSX,
    ABSY,
};
}
