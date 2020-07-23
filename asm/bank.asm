    !include "vera.inc"
    !include "x16.inc"

    !macro jsrf(adr) {
        .PC = *
        !if .PC > 0xa000 {
            pha
            lda BANK_SELECT
            pha
            lda #<adr
            sta jump_adr+1
            lda #>adr
            sta jump_adr+2
            lda #adr>>16
            jmp far_jsr
        }
        lda #adr>>16
        sta BANK_SELECT
        jsr adr & 0xffff
    }

    !section "main", $0801
    !byte $0b, $08,$01,$00,$9e,$32,$30,$36,$31,$00,$00,$00

    lda #1
    sta BANK_SELECT

    ldx $28e
    lda $8c
    sta .ptr+1
    lda $8d
    sta .ptr+2

.ptr
    ldy $ffff,x
    sty filename,x
    cpy #'.'
    bne +
    tya
    dex
    bne .ptr
    ; a is dot offset
    !rept 16 { !byte 0 {
.ext
    !text ".dat"


    SetFileName("BANKED")
    jsr kernel_load

    jsrf call_me
    jsrf start

    lda #1
    sta BANK_SELECT
    stx $1000
    sty $1001
    cli
    rts

far_jsr:
    sta BANK_SELECT
    pla
jump_adr
    jsr $ffff
    pla
    sta BANK_SELECT
    rts


    !section "bank1", $01a000

    nop
call_me:
    ldx #2
    jsrf other
    rts

    !section "bank2", $02a000
start:
    ldy #1
    rts
other:
    ldy #9
    rts

    !section "bank1"

    nop
more_code:
    lda #99
    jsr call_me

