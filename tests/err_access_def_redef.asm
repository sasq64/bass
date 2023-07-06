    jsr testlabel
    rts

testlabel:
    nop
    rts

;!error already defined
testlabel:
    rts
