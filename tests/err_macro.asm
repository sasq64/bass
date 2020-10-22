;
;
;
!macro apa(x) {
    ;!error Data
    lda x
}

start:
    nop
    apa("hey")
    rts

