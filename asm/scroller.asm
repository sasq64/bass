
    !include "text.inc"

    !section "main", $8000

    TextRam = $0400
    ColorRam = $d800

    Line = $0400+0*40

    areg = $2
    treg = $4

    !macro ladr(adr) {
        lda #<adr
        sta areg
        lda #>adr
        sta areg+1
    }

start:

    lda #0
    sta Regs.WinX
    lda #0
    sta Regs.WinY
    ladr floppy
    jsr copy_pet
    lda #40
    sta Regs.WinX
    ladr play
    jsr copy_pet
    
    lda #0
    sta Regs.WinX
    lda #25
    sta Regs.WinY
    lda #80
    sta Regs.WinW
    lda #2
    sta Regs.WinH

    lda #1
    ldx #$20
    !rept 80 {
    sta $d800+i
    stx $400+i
    }
        

loop:
    !rept 79 {
        lda Line+i+1
        sta Line+i
    }

.x  lda text
    sta Line+79
    inc .x+1
    CheckQuit()
    Wait(30)
    jmp loop


copy_pet:

    lda #<TextRam
    sta treg
    lda #>TextRam
    sta treg+1

    ldy #0
    ldx #8

$   lda (areg),y
    sta (treg),y
    iny
    bne -

    inc areg+1
    inc treg+1

    cpx #5
    bne .x

    lda #<ColorRam
    sta treg
    lda #>ColorRam
    sta treg+1
.x
    dex
    bne -

    rts

old:
    ldx #0
.l
    !rept 4 {
;        lda screen+i*250,x
 ;       sta $400+i*250,x
  ;      lda colors+i*250,x
   ;     sta $d800+i*250,x
    }
.s
    inx
    cpx #250
    bne .l

floppy:
    load("../data/petscii/floppy.c64")
    !fill $[:1000]
    !fill 24
    !fill $[1000:]
    !fill 24
play:
    load("../data/petscii/pilt.c64")
    !fill $[:1000]
    !fill 24
    !fill $[1000:]
    !fill 24



    !align 256
    !encoding "screencode_upper"
text:
    !text "THIS ♥ IS A SCROLLER THAT EXEMPLIFIES A VERY SIMPLE ROUTINE "
    !text "RENDERING ON THE TEXT CONSOLE. "
