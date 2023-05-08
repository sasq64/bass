
!macro BasicStart() {
    !assert * == $0801
    !byte $0b,$08,$01,$00,$9e,str(.start),$00,$00,$00
.start
}

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

!macro FastCopy(dest, src, len) {

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
        beq .c
        jmp .a
    }
    .c
    !if .rest > 0 {
        ldx #.rest
    .b
        lda src + .blocks * $100 - 1,x
        sta dest + .blocks * $100 - 1,x
        dex
        bne .b
    }
}

!macro MemCpyForward(dest, src, len) {

    .blocks = len >> 8
    .rest = len & 0xff

    .src = src - .rest
    .dest = dest - .rest

    !trace "MEMCPY"
    !trace src
    !trace len

    lda #.dest>>8
    sta $17
    lda #.dest&0xff
    sta $16

    lda #.src>>8
    sta $19
    lda #.src&0xff
    sta $18

    ldy #.rest
    ldx #.blocks+1
.l    
    lda ($18),y
    sta ($16),y
    ;!run { fmt("TO: {:04x}", mem_read(0x16) + mem_read(0x17) * 256 + reg_y()) }
    ;!run { fmt("FROM: {:04x}", mem_read(0x18) + mem_read(0x19) * 256 + reg_y()) }
    iny
    bne .l

    inc $19
    inc $17
    dex
    bne .l
}

!macro MemCpyBack(dest, src, len) {

    .blocks = len >> 8
    .rest = len & 0xff

    .dest = dest + len - .rest
    .src = src + len - .rest

    !trace "MEMCPY"
    !trace src
    !trace len

    lda #.dest>>8
    sta $17
    lda #.dest&0xff
    sta $16

    lda #.src>>8
    sta $19
    lda #.src&0xff
    sta $18

    ldy #.rest-1
    ldx #.blocks+1
.l    
    lda ($18),y
    sta ($16),y
    ;!run { fmt("TO: {:04x}", mem_read(0x16) + mem_read(0x17) * 256 + reg_y()) }
    ;!run { fmt("FROM: {:04x}", mem_read(0x18) + mem_read(0x19) * 256 + reg_y()) }
    dey
    bne .l
    lda ($18),y
    sta ($16),y
    ;!run { fmt("TO: {:04x}", mem_read(0x16) + mem_read(0x17) * 256 + reg_y()) }
    ;!run { fmt("FROM: {:04x}", mem_read(0x18) + mem_read(0x19) * 256 + reg_y()) }
    dey
    dec $19
    dec $17
    dex
    bne .l
}

!macro MemMove(dest, src, len)
{
    !if (src + len < dest) || (dest + len < src) {
        !print "FastCopy"
        FastCopy(dest, src, len)
    } else {
        !if dest > src {
            !print "CopyBackward"
            MemCpyBack(dest, src, len)
        } else {
            !print "CopyForward"
            MemCpyForward(dest, src, len)
        }
    }
}

