    !include "vera.inc"

    !section "main", $801
    !byte $0b, $08,$01,$00,$9e,str(start),0,0,0
start:
    sei

    ; Clear VRAM
    SetVReg(0)
    SetVAdr($0000 | INC_1)

    ldx #0
    ldy #$40
-
    lda #32
    sta DATA0
    lda #$01
    sta DATA0
    dex
    bne -
    dey
    bne -

main_loop:
    jsr rasters
    lda #1
    sta $9fb5

    jmp main_loop

rasters:
    ; Point to color
    SetVReg(0)
    SetVAdr($1fa00)
    SetVReg(1)
    SetVAdr($1fa01)

    lda #0
    ldx #0
    ldy #0
-   sta raster_lo,y
    sta raster_hi,y
    dey
    bne -

    ldx ypos
    jsr draw_red

    ldx ypos+1
    jsr draw_green

    ldx ypos+2
    jsr draw_blue

    inc ypos
    dec ypos + 1
    inc ypos+2
    inc ypos+2

    ldy #0

    WaitLine(0)

$   NextLine()
    lda raster_lo,y
    sta DATA0
    lda raster_hi,y
    sta DATA1
    iny
    bne -

$   NextLine()

    lda raster_lo,y
    sta DATA0
    lda raster_hi,y
    sta DATA1
    iny
    cpy #200
    bne -

    rts

ypos: !byte 0,0,0

draw_red:
    ldy #red_end-red-1
.loop:
    lda red,y
    sta raster_hi,x
    inx
    dey
    bne .loop
    rts

draw_green:
    ldy #green_end-green -1
.loop:
    lda raster_lo,x
    ora green,y
    sta raster_lo,x
    inx
    dey
    bne .loop
    rts

draw_blue:
    ldy #blue_end-blue-1
.loop:
    lda raster_lo,x
    ora blue,y
    sta raster_lo,x
    inx
    dey
    bne .loop
    rts

raster_lo=$1000
raster_hi=$1100

RSIZE=150

red:
    !rept RSIZE { !byte (sin(i*Math.Pi/RSIZE))*15 }
red_end:

green:
    !rept RSIZE { !byte ((sin(i*Math.Pi/RSIZE))*15)<<4 }
green_end:

blue:
    !rept RSIZE { !byte (sin(i*Math.Pi/RSIZE))*15 }
blue_end:


