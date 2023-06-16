!macro Test() {
macro_label:
    nop
    rts
}

    jsr macro_label
    rts

    Test()

;!error already
macro_label:
    rts
