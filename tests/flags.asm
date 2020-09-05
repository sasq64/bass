
    !org $c000

start:
    ldx #0
    txa
    bne .skip
    ldx #3
.skip
    !check X == 3

    lda #0
    php
    pla
    and #2
    beq .skip2
    ldx #5
.skip2
    !check X == 5
    rts

    !test {
    jsr start
}