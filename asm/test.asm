
    !include "vera.inc"
    !include "x16.inc"

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
    sta VSCALE

;    BorderOn()

    lda #0
    sta BORDER

    LoadColors(colors)

    SetVReg(0)
    SetVAdr($0000 | INC_1)
    jsr copy_image

    SetVReg(0)
    jsr copy_indexes


    lda #0
    sta L1_TILEBASE
    lda #$11 | 0 | 3
    sta L1_CONFIG

    lda #$1e000>>9
    sta L1_MAPBASE

vbl:


    WaitLine(10)

    jsr scale_effect
    ;ldx #20
    ;sta HSCALE

    WaitLine(400)
    lda #5
    sta BORDER

    WaitLine(432)
    lda #0
    sta BORDER

    jmp vbl

copy_indexes:

    SetVAdr($1e000 | INC_1)

    ldy #80
    ldx #0
.loop
    lda indexes,x
    sta DATA0
    dey
    bne +

    ldy #80
    lda #128-80
    clc
    adc ADDR_L
    sta ADDR_L
    bcc +
    inc ADDR_M
$
    inx
    bne .loop
    inc .loop+2
    lda .loop+2
    cmp  #(indexes_end>>8)
    bne .loop

    rts

!test index_test {
    jsr copy_indexes
}

; ---------------------------------------------------------------------------


; Copy image data to VRAM
copy_image:
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
    rts

scale_effect:
    inc sinptr
    bne +
    inc sinptr+1
    ;lda #(scales&0xff)
    ;sta sinptr
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
    sta L1_HSCROLL_L
    dey
    bne loop3

    rts

!test pixel_test {
    jsr copy_image
}


    UTILS()

sinptr:
    !word scales

scales:
    !rept 512 { !byte (sin(i*Math.Pi*2/256)+1) * 15 }

fname:
    !byte "IMAGE", 0
fname_end:

save: !byte 0,0

    !section "indexes", *
indexes:
    !fill png.indexes
indexes_end:
    !section "colors", *
colors:
    !fill png.colors

    !section "IMAGE", 0xa000, NO_STORE|TO_PRG
pixels:
    !fill png.tiles
