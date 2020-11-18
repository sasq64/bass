
    !include "vera.inc"

!macro Print(xpos,ypos,text) {
    SetVAdr(256*ypos + xpos*2 | INC_1);
    ldy #$01
    ldx #0
    beq .start
.loop
    sta DATA0
    sty DATA0
    inx
.start
    lda text,x
    bne .loop
}

    !section "main", $801

	!byte $0b, $08,$01,$00,$9e,$32,$30,$36,$31,$00,$00,$00
  start:
    sei
    lda #32
    ;sta 0x9f2a
    ;sta $9f2b
    ; Clear VRAM
    SetVReg(0)
    SetVAdr($0000 | INC_1)

    ldx #0
    ldy #$40
.loop
    lda #32
    sta DATA0
    lda #$01
    sta DATA0

    dex
    bne .loop
    dey
    bne .loop

    lda #1
    jsr $ff62

    Print(3,3,hello)

    ; Prepare for raster loop
    lda #0b10
    sta IEN


main_loop:

    ;jsr rasters

    WaitLine(100)

    lda scrollpos
    sta $9f37
    sta $9f32
    inc scrollpos


    jmp main_loop


scrollpos: !byte 45

hello:
    !byte "Hedllo People",0

rasters:
    ; Point to color
    SetVReg(0)
    SetVAdr($1fa00)
    SetVReg(1)
    SetVAdr($1fa01)

    ; Prepare for raster loop
    lda #2
    sta IEN

loop1:
    lda #0
    ldx #0
    ldy #0
.loop:
    sta raster_lo,y
    sta raster_hi,y
    dey
    bne .loop

    ldx ypos
    jsr draw_red

    ldx ypos+1
    jsr draw_green

    ldx ypos+2
    jsr draw_blue

    inc ypos
    dec ypos + 1
    inc ypos+2
    inc ypos+2

    lda #2
    sta ISR

    lda #10
    sta IRQLINE_L

    ldy #240

.loop2:
    lda ISR
    and #$2
    beq .loop2
    sta ISR
    lda raster_lo,y
    sta DATA0
    lda raster_hi,y
    sta DATA1

    inc $9f28
    dey
    bne .loop2

    ;inc $9f37

    lda #0
    sta DATA0
    sta DATA1

    ;  jsr scroller

    jmp loop1
    rts

scroller:

    clc 
    lda $9f37
    adc #1
    cmp #8
    beq .skip
    sta $9f37
    rts
.skip:
    lda #0
    sta $9f37
    rts


copy:
    lda #0
    sta $9f25
    lda #1
    sta $9f22
    lda #$fa
    sta $9f21
    lda #0
    sta $9f20
        
    lda #1
    sta $9f25
    lda #1
    sta $9f22
    lda #$fa
    sta $9f21
    lda #1
    sta $9f20

    lda $9f23
    sta $9f24




textpos: !byte 0

text !byte 1,2,3,4,5,64,65,66,67,128,129,130,0

ypos: !byte 0,0,0

draw_red:
    ldy #red_end-red-1
.loop:
    lda red,y
    sta raster_hi,x
    inx
    sta raster_hi,x
    inx
    sta raster_hi,x
    inx
    dey
    bne .loop
    rts



draw_green:
    ldy #green_end-green -1
.loop:
    lda raster_lo,x
    ora green,y
    sta raster_lo,x
    inx
    sta raster_lo,x
    inx
    sta raster_lo,x
    inx
    dey
    bne .loop
    rts

draw_blue:
    ldy #blue_end-blue-1
.loop:
    lda raster_lo,x
    ora blue,y
    sta raster_lo,x
    inx
    sta raster_lo,x
    inx
    sta raster_lo,x
    inx
    dey
    bne .loop
    rts

raster_lo=$1000
raster_hi=$1100

!test "draw_blue"
    ldx #0
    jsr draw_blue

red:
    !byte 0,2,4,8,10,12,13,14,15,15,14,13,12,10,8,4,2,0
red_end:

green:
    !byte 0, $20,$40,$80,$a0,$c0,$d0,$e0,$f0,$f0,$e0,$d0,$c0,$a0,$80,$40,$20,0
green_end:

blue:
    !byte 0,2,4,8,10,12,13,14,15,15,14,13,12,10,8,4,2,0
blue_end:


