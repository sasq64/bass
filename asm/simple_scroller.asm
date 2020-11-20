    !include "text.inc"

    !section "main", $8000

    Line = $0400+24*40

    Border(LightBlue)
    Clear(LightBlue, Blue)
loop:
    !rept 39 {
        lda Line+i+1
        sta Line+i
    }

.x  lda text
    sta Line+39
    inc .x+1
    CheckQuit()
    Wait(500)
    jmp loop

    !align 256
text:
    !encoding "screencode_upper"
    !text "THIS IS A SCROLLER THAT EXEMPLIFIES A VERY SIMPLE ROUTINE "
    !text "RENDERING ON THE TEXT CONSOLE. "
