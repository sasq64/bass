
    !section "a", $1000
first:
    nop
$   lda $c001
    !check A==30
    cmp #30
    bne -
    tax
    lda $c000
    !check A==0
    beq +
    jmp -
$   rts
    nop

    !section "data", $c000
    !byte 0,30

    !section "b", $2000
second
    nop
loop:
    lda $d012
    cmp #30
    bne loop
    lda $c000
    beq skip
    jmp loop
skip
    rts
$   nop

    !section "c", $3000
    !macro dummy(a) {
.x  lda $d012
    cmp #30
    bne .x
    lda $c000
    beq .y
    jmp .x
.y  rts
}
$   nop
    dummy(0)
$   nop


many:
    clc

    ldx #0
    bcc +++
    nop
$
    bcc .out
$
    inx
    bcc .out
$
    ldx #3
    bcc --
    rts
.out
    rts

    !test "labels"
        !log "We are here"
        jsr first
        ;!log "X=",X
        jsr many
        !check X == 4
        nop
        rts
