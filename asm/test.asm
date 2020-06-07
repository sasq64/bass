
    !include "vera.inc"
    !include "x16.inc"

    !enum Vera { 
        DATA0 = 0x1234
        DATA1
        DATA2
    }

    !enum {
        XXX
        YYY
        ZZZ = "Yo"
    }


    png = load_png("../data/face.png")

    !enum Image {
        pixels = png.pixels
        width = 320
        height = 200
    }

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
    sta VSCALE

;    BorderOn()

    lda #0
    sta BORDER

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

    lda #0
    sta L1_TILEBASE
    lda #4 | 3
    sta L1_CONFIG


vbl:


    WaitLine(10)

    ;jsr scale_effect
    ;ldx #20
    ;sta HSCALE



    WaitLine(400)
    lda #5
    sta BORDER

    WaitLine(432)
    lda #0
    sta BORDER

    jmp vbl

scale_effect:
    inc sinptr
    bne +
    ;inc sinptr+1
    lda #(scales&0xff)
    sta sinptr
$
    lda sinptr
    sta ptr+1
    lda sinptr+1
    sta ptr+2


    ldx #0
    ldy #0
loop3
    NextLine()
ptr:
    lda scales,x
    inx
    bne +
    inc ptr+2
$
    sta HSCALE
    dey
    bne loop3

    rts


sinptr:
    !word scales

scales:
    !rept 512 { !byte (sin(i*Math.Pi*2/256)+1) * 5 + 59 }

fname:
    !byte "IMAGE", 0
fname_end:

save: !byte 0,0

colors:
    !block png.colors
    !section "IMAGE", 0xa000, NO_STORE|TO_PRG
pixels:
    !block png.pixels
