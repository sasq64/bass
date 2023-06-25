    jsr testlabel
    rts

testlabel:
    nop
    rts

;!error already
testlabel:
    rts
