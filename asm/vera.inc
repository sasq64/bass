
INC_1 = $100000
INC_2 = $200000

!macro SetVAdr(adr) {
    lda #(adr & $ff)
    sta $9f20
    !if ((adr & 0xff) != ((adr>>8) & 0xff)) {
        lda #(adr>>8)
    }
    sta $9f21
    lda #(adr>>16)
    sta $9f22
}

!macro SetVReg(reg) {
    lda #reg
    sta $9f25
}

!macro WaitLine(ypos) {
    lda #$02 | ((ypos>>1)&$80)
    sta IEN
    lda #ypos
    sta IRQLINE_L
.loop:
    lda ISR
    and #$2
    beq .loop
    sta ISR
}

!macro NextLine() {
    inc IRQLINE_L
    bne .loop
    lda #$82
    sta IEN
.loop
    lda ISR
    and #$2
    beq .loop
    sta ISR
}

!macro LoadColors(colors) {
    SetVReg(0)
    SetVAdr($1fa00 | INC_1)
    ldx #0
.cl
    lda colors,x
    sta DATA0
    inx
    bne .cl
.cl2
    lda colors+256,x
    sta DATA0
    inx
    bne .cl2
}

!macro BorderOn() {
    lda #2
    sta CTRL
    lda #8
    sta DC_HSTART
    lda #0
    sta CTRL
}


VREG0=$9f23
VREG1=$9f24

DATA0=$9f23
DATA1=$9f24
IRQLINE_L=$9f28
IEN=$9f26
ISR=$9f27
BORDER=$9f2c
CTRL=$9f25
HSCALE=$9f2a
VSCALE=$9f2b
L0_MAPBASE=$9f2e
L0_TILEBASE=$9f2f
L1_MAPBASE=$9f35
L1_TILEBASE=$9f36
L1_CONFIG=$9f34
L1_VSCROLL_L=$9f39
L1_VSCROLL_H=$9f3a
L1_HSCROLL_L=$9f37
L1_HSCROLL_H=$9f38

DC_HSTART=$9f29

BANK_SELECT=$9f61

SCALE_1X = 128
SCALE_2X = 64
SCALE_3X = 48
SCALE_4X = 32

