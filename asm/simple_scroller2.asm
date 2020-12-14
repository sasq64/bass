    !include "pet100.inc"

    !section "main", $8000

    Line = $0400


start:
    lda #0
    sta Regs.WinX
    lda #CONSOLE_HEIGHT-2
    sta Regs.WinY
    lda #1
    sta Regs.WinH
    lda #CONSOLE_WIDTH
    sta Regs.WinW


    lda #<Palette
    sta $02
    lda #>Palette
    sta $03

    lda #0
    tay
    clc
$
    !rept 3 {
        sta ($02),y
        inc $02
    }
    adc #16
    bne -
    
    ldx #CONSOLE_WIDTH
$
    dex
    lda colors,x
    sta $d800,x
    cpx #0
    bne -

.res
    lda #<text
    sta .x+1
.loop:
    jsr scroll

.x  lda text
    beq .res
    sta Line+CONSOLE_WIDTH-1
    inc .x+1
    CheckQuit()
    Wait(4)
    jmp .loop

scroll:
    !rept CONSOLE_WIDTH-1 {
        lda Line+i+1
        sta Line+i
    }
    rts

    !align 256
text:
    !encoding "screencode_upper"
    !text " -= THIS IS A SCROLLER THAT EXEMPLIFIES A VERY SIMPLE ROUTINE "
    !text "RENDERING ON THE TEXT CONSOLE.   "
    !byte 0
colors:
    !fill 14, [i -> i]
    !fill CONSOLE_WIDTH-28, 15
    !fill 14, [i -> 15-i]
