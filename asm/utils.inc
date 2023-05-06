
!macro MemSet(dest, val, len) {

    .blocks = len >> 8
    .rest = len & 0xff

    !print .blocks

    lda #val

    !if .blocks > 0 {
        ldx #0
    .a   
        !rept .blocks {
            sta dest + i * $100, x
        }
        dex
        bne .a
    }

    !if .rest > 0 {
        ldx #.rest
    .b
        sta dest + .blocks * $100 - 1, x
        dex
        bne .b
    }
}

!macro MemCpy(dest, src, len) {

    .blocks = len >> 8
    .rest = len & 0xff

    !if .blocks > 0 {
        ldx #0
    .a   
        !rept .blocks {
            lda src + i * $100, x
            sta dest + i * $100, x
        }
        dex
        bne .a
    }

    !if .rest > 0 {
        ldx #.rest
    .b
        lda src + .blocks * $100 - 1,x
        sta dest + .blocks * $100 - 1,x
        dex
        bne .b
    }
}
