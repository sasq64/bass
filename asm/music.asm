
    !section "text", $880 ; section.main.end

    data = load("../data/test.sid")
    sid = sid_parse(data)

!macro print(txt, xpos, ypos) {
    .LEN = .text_end - .text
    ldx #.LEN-1
.l  lda .text,x
    sta $400+xpos+ypos*40,x
    dex
    bpl .l
    !section in="text" {
.text:
    !fill txt
.text_end:
    }
}

    !section "main", $801

    !byte $0b, $08,$01,$00,$9e,$32,$30,$36,$31,$00,$00,$00
start
    lda #0
    sta $d020
    sta $d021
    jsr $e544
    lda #23
    sta $d018
    sei

    !chartrans "♥", 0x66

    print("Playing SID ♥", 1, 1)
    print(sid.title, 1, 2)
    print(sid.composer, 1, 3)

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
