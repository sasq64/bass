
    !include "vera.inc"
    !include "x16.inc"

    !enum Vera { 
        DATA0 = 0x1234
        DATA1
        DATA2
    }

    !enum  {
        XXX
        YYY
        ZZZ
    }


    png = load_png("../data/face.png")

    !section "main", $801

    !byte $0b, $08,$01,$00,$9e,$32,$30,$36,$31,$00,$00,$00
  start:
    lda #0
    sta BANK_SELECT

    LoadFile(fname)

    lda #0
    sta BANK_SELECT

    sei
    lda #SCALE_2X
    sta HSCALE
    lda #SCALE_2X
    sta VSCALE

    lda #2
    sta IEN
    lda #0
    sta BORDER

    ; Active area
    lda #2
    sta CTRL
    lda #2
    sta $9f29
    lda #158
    sta $9f2a
    lda #0
    sta CTRL

    LoadColors(colors)

    SetVReg(0)
    SetVAdr($0000 | INC_1)

    lda #0
    sta BANK_SELECT
.loop2
    ldx #0
    ldy #32
    lda #pixels>>8
    sta .loop+2
.loop
    lda pixels,x
    sta DATA0
    inx
    bne .loop
    inc .loop+2
    dey
    bne .loop
    inc BANK_SELECT
    lda #8
    cmp BANK_SELECT
    bne .loop2

go:
    lda L1_TILEBASE
    sta save
    lda L1_CONFIG
    sta save+1

vbl:
    WaitLine(0)
    lda #0
    sta L1_VSCROLL_H
    sta L1_VSCROLL_L
    lda #0
    sta L1_TILEBASE
    lda #4 | 3
    sta L1_CONFIG

    lda #$80
    sta L1_HSCROLL_L


    WaitLine(400)
    lda #5
    sta BORDER

    lda save
    sta L1_TILEBASE
    lda save+1
    sta L1_CONFIG
    lda #$f
    sta L1_VSCROLL_H
    lda #50
    sta L1_VSCROLL_L

    WaitLine(432)
    lda #0
    sta BORDER

    jmp vbl

fname:
    !byte "IMAGE", 0
fname_end:

save: !byte 0,0

colors:
    !block png.colors
    !section "IMAGE", 0xa000, NO_STORE|TO_PRG
pixels:
    !block png.pixels
