;----------------------------------------------------------
; badass -- C64 Example source
; Koala screen, animated sprite and music
; sasq - 2020
;----------------------------------------------------------
!script "../lua/sid.lua"

!include "utils.inc"
!include "vic.inc"

musicLocation = $1000
    !section "RAM",$801

    !section "code",in="RAM" {
    BasicStart()

    sei

    !test "copy"
    MemMove($1000, sections.music.start, sections.music.size)

    ;lda #$57
    ;sta $2000
    !rts
    !assert compare(tests.copy.ram[$1000:$1000+sections.music.size], sections.music.data)

    lda #0
    tax
    tay
    jsr $1000

$   lda #100
    cmp $d012
    bne -
    lda #4
    sta $d020
    jsr $1003
    lda #0
    sta $d020
    jmp -

    !include "../external/lzsa/asm/6502/decompress_fast_v1.asm"
    }
    sid = load("../data/test.sid")
    music_init = big_word(sid[$a:$c])
    music_play = big_word(sid[$c:$e])

    !assert music_init == musicLocation

    music_data = sid[$7e:]

    !section "music", in="RAM" {
        !fill music_data
    }

