    !include "pet100.inc"

    !section "main", $c000

    Line = $0400+24*40

    ;Border(LightBlue)
   jmp $fce2

    lda #32 + 128
    sta $400

    lda #32
    sta $400
    jmp *

loop:
    !rept 39 {
        lda Line+i+1
        sta Line+i
    }

.x  lda text
    sta Line+39
    inc .x+1
    CheckQuit()
    Wait(4)
    jmp loop

    !align 256
text:
    !encoding "screencode_upper"
    !text "THIS IS A SCROLLER THAT EXEMPLIFIES A VERY SIMPLE ROUTINE "
    !text "RENDERING ON THE TEXT CONSOLE. "
