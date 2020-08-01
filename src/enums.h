#pragma once

#include <cstdint>

namespace sixfive {

// Addressing modes
enum AddressingMode : uint8_t
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
    INDZ,
    ABS,
    ABSX,
    ABSY,
    ZP_REL,
};
} // namespace sixfive
