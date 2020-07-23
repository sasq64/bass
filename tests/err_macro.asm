;
;
;
!macro apa(x) {
    ;!error Data type
    lda x
}

start:
    nop
    apa("hey")
    rts

