
    data = load("../data/test.sid")
    sid = sid_parse(data)

    !section "main", $801

    !byte $0b, $08,$01,$00,$9e,$32,$30,$36,$31,$00,$00,$00
start
    sei
    lda #0
    jsr sid.init
.loop
    lda $d012
    cmp #130
    bne .loop
    lda #2
    sta $d020
    jsr sid.play
    lda #0
    sta $d020
    jmp .loop


    !section "music", sid.load
music:
    !fill sid.data
