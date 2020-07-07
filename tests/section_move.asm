
    !section "utils", section.main.end
print:
    bcc .skip
    lda #45
    jsr $ff00
.skip
    rts

    !section "main", 0

    nop
    nop
    jsr print
    rts
after_rts:

!assert after_rts == print
