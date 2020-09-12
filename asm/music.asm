
    !section "text", $880

    !script "../lua/sid.lua"

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

    !byte $0b,$08,$01,$00,$9e,str(start),$00,$00,$00
start:
    lda #0
    sta $d020
    sta $d021
    jsr $e544
    ;lda #23
    ;sta $d018
    sei

    !chartrans "♥*", 0x53,0x51

    print("* playing * sid ♥", 1, 1)
    print(to_lower(sid.title), 1, 2)
    print(to_lower(sid.composer), 1, 3)

    lda #0
    jsr sid.init
$
    lda $d012
    cmp #130
    bne -
    lda #2
    sta $d020
    jsr sid.play
    lda #0
    sta $d020
    jmp -

    !section "music", sid.load
music:
    !fill sid.data

    !script last {

    frame = 0

    map_bank_write(0xd4, 1, function(adr, val)
        print("SID", adr - 0xd400, val)
    end)

    for 1 to 10*50 do
        call(sym("sid.play"))
        frame++
    end

    }