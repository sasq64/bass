
    !cpu 6502

!section "main", $c000
test_lax:
    lax data
    !check A == $77
    !check X == $77
    lda #$f0
    sax data2
    ldy data2
    !check Y == $70

    rts

test_nop:

    nop #3
    nop $01
    nop $1234
    rts

data:
    !byte $77
data2:
    !byte 0


!test "illegals" {
    jsr test_lax
    jsr test_nop
}