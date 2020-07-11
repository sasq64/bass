
    !section "a", $1000
first:
    nop
$   lda $c001
    !check A==30
    cmp #30
    bne -
    tax
    lda $c000
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

    !test {
        !log "We are here"
        jsr first
        !log "X=",X
        nop
    }
