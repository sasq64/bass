
    !include "text.inc"

    !section "main", $8000

    Line = $0400+24*40
start:

    ldx #0
.l
    !rept 4 {
        lda screen+i*250,x
        sta $400+i*250,x
        lda colors+i*250,x
        sta $d800+i*250,x
    }
.s
    inx
    cpx #250
    bne .l

loop:
    !rept 39 {
        lda Line+i+1
        sta Line+i
    }

.x  lda text
    sta Line+39
    inc .x+1
    CheckQuit()
    Wait(100)
    jmp loop


    load("../data/petscii/floppy.c64")
screen:
    !fill $[:1000]
colors:
    !fill $[1000:]


    !align 256
    !encoding "screencode_upper"
text:
    !text "THIS ♥ IS A SCROLLER THAT EXEMPLIFIES A VERY SIMPLE ROUTINE "
    !text "RENDERING ON THE TEXT CONSOLE. "
