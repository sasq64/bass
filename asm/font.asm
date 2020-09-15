!script "../lua/sid.lua"

; Set the 16k VIC area. Can only point
; to $0000, $4000, $8000 or $c000
!macro VicAdr(adr) {
    !assert (adr & $3fff) == 0
    lda #((~adr)&$ffff)>>14
    sta $dd00
}

; Set the Bitmap and Screen offsets within VIC
!macro BitmapAndScreen(bm_offs, scr_offs) {
    !assert (bm_offs & $dfff) == 0
    !assert (scr_offs & $c3ff) == 0
    .bits0 = (bm_offs>>10)
    .bits1 = (scr_offs>>6)
    lda #.bits0 | .bits1
    sta $d018
}


    !section "RAM",$801
    !section "main",in="RAM",start=$801
    !section "data",in="RAM"

    !section "code",in="main"
    !byte $0b,$08,$01,$00,$9e,str(start),$00,$00,$00
start:

    sei

    lda #$31
    sta $1

    ldy #8
    ldx #0
.loop
    lda font,x
    sta $2000,x
    inx
    bne .loop
    inc .loop+2
    inc .loop+5
    dey
    bne .loop

    lda #$37
    sta $1

    lda #0
    sta $d020
    sta $d021

    lda #$18
    sta $d018

    cli
    rts

font:
    png = load_png("../data/ff.png")
    tiles = layout_tiles(png.pixels, 128/8, 1, 8)

    tiles2 = index_tiles(tiles, 8)

    !fill tiles2.tiles[8*32:8*64]
    !fill tiles2.tiles[0:8*32]
