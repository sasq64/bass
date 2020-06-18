
    !include "vera.inc"
    !include "x16.inc"

    png = load_png("../data/face.png")

    !section "main", $801

    !byte $0b, $08,$01,$00,$9e,$32,$30,$36,$31,$00,$00,$00
  start:
    lda #0
    sta BANK_SELECT

    LoadFile(fname)

    lda #0
    sta BANK_SELECT

    sei
    lda #SCALE_2X
    sta HSCALE
    sta VSCALE

;    BorderOn()

    lda #0
    sta BORDER

    LoadColors(colors)

    SetVReg(0)
    SetVAdr($0000 | INC_1)
    jsr copy_image

    SetVReg(0)
    jsr copy_indexes


    lda #0
    sta L1_TILEBASE
    lda #$11 | 0 | 3
    sta L1_CONFIG

    lda #$1e000>>9
    sta L1_MAPBASE

vbl:


    WaitLine(10)

    jsr scale_effect
    ;ldx #20
    ;sta HSCALE

    WaitLine(400)
    lda #5
    sta BORDER

    WaitLine(432)
    lda #0
    sta BORDER

    jmp vbl

%{

    vram = { 0 }
    vregs = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}
    vsel = 1
    vadr = { 0, 0 }
    vinc = { 0, 0 }
    increments = {
        0,   0,
        1,   -1,
        2,   -2,
        4,   -4,
        8,   -8,
        16,  -16,
        32,  -32,
        64,  -64,
        128, -128,
        256, -256,
        512, -512,
        40,  -40,
        80,  -80,
        160, -160,
        320, -320,
        640, -640,
    }

    set_break_fn(2, function(what)
        print("Break 2")
    end)

    map_bank_read(0x9f, 1, function(adr)
        offset = adr & 0xff
        -- print("Read", offset)
       if offset >= 0x20 and offset < 0x40 then
            if offset == 0x20 then
                return vadr[vsel] & 0xff
            elseif offset == 0x21 then
                return (vadr[vsel] >> 8) & 0xff
            elseif offset == 0x22 then
                return (vadr[vsel] >> 16) | (vinc[vsel]<<3)
            end
            res =  vregs[offset-0x20+1]
            return res
       else
            return mem_read(0x9f00 | offset)
       end
    end)

    map_bank_write(0x9f, 1, function(adr, val)
        offset = adr & 0xff
       if offset >= 0x20 and offset < 0x40 then
            if offset == 0x20 then
                vadr[vsel] = (vadr[vsel] & 0x1ff00) | val
            elseif offset == 0x21 then
                vadr[vsel] = (vadr[vsel] & 0x100ff) | (val<<8)
            elseif offset == 0x22 then
                vadr[vsel] = (vadr[vsel] & 0xffff) | ((val&1)<<16)
                vinc[vsel] = val>>3
            elseif offset == 0x23 then
                -- print(string.format("Vram write %x to %x", val, vadr[1]))
                vram[vadr[1]+1] = val
                vadr[1] = vadr[1] + increments[vinc[1]+1]
            elseif offset == 0x24 then
                -- print(string.format("Vram write %x to %x", val, vadr[2]))
                vram[vadr[2]+1] = val
                vadr[2] = vadr[2] + increments[vinc[2]+1]
            end
            -- print(string.format("Write %x to %x", val, offset))
            vregs[offset-0x20+1] = val
       else
            return mem_read(0x9f00 | offset)
       end
    end)

    function get_vram()
        return vram
    end

}%

copy_indexes:

    SetVAdr($1e000 | INC_1)

    ldy #80
    ldx #0
.loop
    lda indexes,x
    sta DATA0
    dey
    bne +

    ldy #80
    lda #128-80
    clc
    ;brk #2
    adc ADDR_L
    sta ADDR_L
    bcc +
    inc ADDR_M
$
    inx
    bne .loop
    inc .loop+2
    lda .loop+2
    cmp  #(indexes_end>>8)
    bne .loop

    rts

; ---------------------------------------------------------------------------


; Copy image data to VRAM
copy_image:
    lda #0
    sta BANK_SELECT
.loop2
    ldx #0
    ldy #32
    lda #pixels>>8
    sta .loop+2
.loop
    lda pixels,x
    sta DATA0
    inx
    bne .loop
    inc .loop+2
    dey
    bne .loop

    inc BANK_SELECT
    lda #8
    cmp BANK_SELECT
    bne .loop2
    rts

scale_effect:
    inc sinptr
    bne +
    inc sinptr+1
    ;lda #(scales&0xff)
    ;sta sinptr
$
    lda sinptr
    sta ptr+1
    lda sinptr+1
    sta ptr+2


    ldx #0
    ldy #0
loop3
    NextLine()
ptr:
    lda scales,x
    inx
    bne +
    inc ptr+2
$
    sta L1_HSCROLL_L
    dey
    bne loop3

    rts

;!test pixel_test {
;    jsr copy_image
;}


    UTILS()

sinptr:
    !word scales

scales:
    !rept 512 { !byte (sin(i*Math.Pi*2/256)+1) * 15 }

fname:
    !byte "IMAGE", 0
fname_end:

save: !byte 0,0

    !section "indexes", *
indexes:
    !fill png.indexes
indexes_end:

;-------------------
!test index_test {
    jsr copy_indexes
}
; Check first 2 line of screen tiles
vram = get_vram()
!assert compare(vram[0x1e000:0x1e000+80], png.indexes[0:80])
!assert compare(vram[0x1e080:0x1e080+80], png.indexes[80:160])
;--------------------

    !section "colors", *
colors:
    !fill png.colors

    !section "IMAGE", 0xa000, NO_STORE|TO_PRG
pixels:
    !fill png.tiles




