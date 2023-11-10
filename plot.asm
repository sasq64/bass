/// @file
/// C standard library stdint.h
///
/// Defines a set of integral type aliases with specific width requirements, along with macros specifying their limits and macro functions to create values of these types.
  // Commodore 64 PRG executable file
.file [name="plot.prg", type="prg", segments="Program"]
.segmentdef Program [segments="Basic, Code, Data"]
.segmentdef Basic [start=$0801]
.segmentdef Code [start=$80d]
.segmentdef Data [startAfter="Code"]
.segment Basic
:BasicUpstart(main)
.segment Code
main: {
    ldx #0
  __b1:
    cpx #$64
    bcc __b2
    rts
  __b2:
    txa
    asl
    asl
    tay
    txa
    sta.z put_pixel.x
    lda #0
    sta.z put_pixel.x+1
    jsr put_pixel
    inx
    jmp __b1
}
// void put_pixel(__zp(4) unsigned int x, __register(Y) char y)
put_pixel: {
    .label o = 2
    .label o_1 = 3
    .label x = 4
    tya
    lsr
    lsr
    lsr
    sta.z o
    asl
    asl
    clc
    adc.z o
    asl
    asl
    asl
    asl
    asl
    asl
    sta.z o_1
    lda #7
    and.z x
    tay
    lda #$80
    cpy #0
    beq !e+
  !:
    lsr
    dey
    bne !-
  !e:
    // + x&0xfff8;
    ldy.z o_1
    ora $6000,y
    sta $6000,y
    rts
}
