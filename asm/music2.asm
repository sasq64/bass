;----------------------------------------------------------
; badass -- C64 Example source
; Music with relocation
; sasq - 2020
;----------------------------------------------------------

!include "utils.inc"
!include "vic.inc"

musicLocation = $1000
!section "RAM",$801

!section "code",in="RAM" {
    BasicStart()

    sei

    !test "copy"
    MemMove(musicLocation, sections.music.start, sections.music.size)
    !rts
    !assert compare(tests.copy.ram[musicLocation:musicLocation+sections.music.size], sections.music.data)

    lda #0
    tax
    tay
    jsr music_init

$   lda #100
    cmp $d012
    bne -
    
    lda #4
    sta $d020
    jsr music_play
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

