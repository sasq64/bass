
    !section "text", $8a0

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
    ;jsr sid.init
    jsr fast_init
$
    lda $d012
    cmp #130
    bne -
    lda #2
    sta $d020
    ;jsr sid.play
    jsr fast_play
    lda #0
    sta $d020
    jmp -

fast_init:
    lda #<sid_data
    sta $02
    lda #>sid_data
    sta $03
    rts

fast_play:

    ldy #0
.loop
    lda ($02),y
    bmi .last
    tax
    iny
    lda ($02),y
    sta $d400,x
    iny
    bne .loop

.last
    and #$7f
    tax
    iny
    lda ($02),y
    sta $d400,x
    iny

    tya
    clc
    adc $02
    bcc +
    inc $03
$   sta $02

    rts


    !section "music", sid.load
music:
    !fill sid.data

%{

    sid_data = nil

    function generate_data()

        map_bank_write(0xd4, 1, function(adr, val)
            if sid_data then
                table.insert(sid_data, adr - 0xd400)
                table.insert(sid_data, val)
            end
        end)

        start_run()
        init = sym("sid.init")
        play = sym("sid.play")
        set_a(0)
        call(init)
        sid_data[#sid_data - 1] = sid_data[#sid_data - 1] | 0x80
        for i=1,15*50 do
            l = #sid_data
            call(play)
            if l ~= #sid_data then
                sid_data[#sid_data - 1] = sid_data[#sid_data - 1] | 0x80
            else
                print("Silent frame")
            end
        end
    end

    function get_sid_data()
        sid_data = {}
        generate_data()
        result = sid_data
        sid_data = nil
        return result
    end
}%

    !text "SID"
sid_data:
    !fill get_sid_data()
    nop

%{
    map_bank_write(0xd4, 1, function(adr, val)
        print("TEST", adr - 0xd400, val)
    end)
}%

    !test $c000

!test "fast_play" {
    jsr fast_init
    jsr fast_play
    jsr fast_play
    jsr fast_play
    jsr fast_play
}
