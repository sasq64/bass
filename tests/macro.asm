
    !section "x", 0
    yy = 99

!macro apa(x,y) {
    ldx #x
    !rept 5 { nop }
    !if (x ==3) {
        sta $d020
    }
    ldy #y
}

!macro test(a) {
    lda #a
    jsr other
}
    !section "main" , $800
    !test "apa"
    lda #9
a0:
    apa(3,4)
a1:
    !assert (a1-a0) == 12
    !check X == 3
    !check Y == 4
    !check RAM[$d020] == 9
    test(3)
    rts
other:
    rts

