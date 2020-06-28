
INC_1 = $100000
INC_2 = $200000
INC_32 = 6<<20

INC_40 = 11<<20
INC_80 = 12<<20

ADDR_L=$9f20
ADDR_M=$9f21
ADDR_H=$9f22

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

!macro SetL1MapBase(adr) {
    !assert (adr & $1ff) == 0
    lda #(adr>>9)
    sta L1_MAPBASE
}
!macro SetL1Tiles(adr, width, height) {
    !assert width == 8 || width == 16
    !assert height == 8 || height == 16
    !assert (adr & $7ff) == 0
    lda #(adr >> 9) | (width>>4) | ((height>>3)&2)
    sta L1_TILEBASE
}

MAP_WIDTH_32 = $00
MAP_WIDTH_64 = $10
MAP_WIDTH_128 = $20
MAP_WIDTH_256 = $30

MAP_HEIGHT_32 = $00
MAP_HEIGHT_64 = $40
MAP_HEIGHT_128 = $80
MAP_HEIGHT_256 = $c0

DEPTH_1BPP = 0
DEPTH_2BPP = 1
DEPTH_4BPP = 2
DEPTH_8BPP = 3

BITMAP_MODE = 4



!macro SetVAdr(adr) {
    .LO = adr & $ff
    .MID = (adr >> 8) & 0xff
    .HI = adr >> 16
    !if .LO == 0 {
        stz $9f20
    } else {
        lda #.LO
        sta $9f20
    }
    !if .MID == 0 {
        stz $9f21
    } else {
        lda #.MID
        sta $9f21
    }
    !if .HI == 0 {
        stz $9f22
    } else {
        lda #.HI
        sta $9f22
    }
}

!macro SetVReg(reg) {
    !if reg == 0 {
        stz $9f25
    } else {
        lda #reg
        sta $9f25
    }
}

!macro WaitLine(ypos) {
    lda #ypos
    sta IRQLINE_L
    lda #$02 | ((ypos>>1)&$80)
    sta IEN
    !if ypos > 255 {
        lda #2
    }
.loop:
    bit ISR
    beq .loop
    sta ISR
}

!macro NextLine() {
    inc IRQLINE_L
    bne .skip
    lda #$82
    sta IEN
.skip
    lda #2
.loop
    bit ISR
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
    stz CTRL
}


