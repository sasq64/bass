!macro move(val16, adr) {
    lda #(val16&0xff)
    sta adr
    lda #(val16>>8)
    sta adr+1
}


!macro memcpy(dst, src, len) {
    .HL = len>>8
    move(dst, block_copy.a1+1)
    move(src, block_copy.a0+1)
    !rept 4 { ldy  #.HL }
    jsr block_copy
    move(dst+(len - (len>>8)), mem_copy.a1+1)
    move(src+(len - (len>>8)), mem_copy.a0+1)
    ldx  #(len - (len>>8))
    jsr mem_copy
}


    sid = load_sid("data/test.sid")
    ;!org $0801

    !section "main", $801

    !byte $0b, $08,$01,$00,$9e,$32,$30,$36,$31,$00,$00,$00

  start:

    sei
    ;memcpy(sid.load, music, 8320)
    lda #0
    jsr sid.init
.loop
    lda $d012
    cmp #130
    bne .loop
    lda #2
    sta $d020
    jsr sid.play
    lda #0
    sta $d020
    jmp .loop


; y : blocks
block_copy:
    ; copy 256 bytes
    ldx #0
.loop
.a0 lda $1234,x
.a1 sta $1234,x
    inx
.l0 bne .loop
    dey
    beq .done
    inc .a0+1
    inc .a1+1
    jmp .loop
.done
    rts

; x : bytes
mem_copy:
.loop
    dex
.a0 lda $1234,x
.a1 sta $1234,x
    cpx #0
    bne .loop
    rts

    !section "music", sid.load
music:
    !block sid.data

;    !section "main"
;!test copy
;    memcpy(sid.load, music, 8320)
;!endt


