
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

