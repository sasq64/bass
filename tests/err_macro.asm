;
;
;
!macro apa(x) {
    ;!error
    lda x
}

start:
    nop
    apa("hey")
    rts

