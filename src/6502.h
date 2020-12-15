#pragma once

#include <array>
#include <cstdint>

namespace sixfive {

// Addressing modes
enum class Mode : uint8_t
{
    NONE, // rts
    ACC,  // rol a

    IMM,  // lda #$81
    REL,  // beq $8100
    ZP,   // lda ($81)
    ZPX,  // lda $81,x
    ZPY,  // lda $81,y
    INDX, // lda ($81,x)
    INDY, // lda ($81),y
    INDZ, // lda ($81),z
    INDZF,// lda [$81],z

    INDZP,// lda ($81)

    IND,  // jmp ($8100)
    IMM16,
    ABS,  // lda $8100
    ABSX, // lda $8100,x
    ABSY, // lda $8100,y
    ZP_REL,
    REL16,// bsr $8100
};

enum class Reg
{
    A,
    X,
    Y,
    Z,
    SP,
    SR,
    PC
};

enum class CPU
{
    Cpu6502,
    Cpu65C02,
    Cpu4510
};


} // namespace sixfive
