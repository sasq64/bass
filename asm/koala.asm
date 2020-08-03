; C64 Koala viewer

!macro VicAdr(adr) {
    !assert (adr & 0x3fff) == 0
    lda #((~adr)&0xffff)>>14
    sta $dd00
}

!macro BitmapAndScreen(bm_offs, scr_offs) {
    !assert (bm_offs & 0xdfff) == 0
    !assert (scr_offs & 0xc3ff) == 0
    .bits0 = (bm_offs>>10)
    .bits1 = (scr_offs>>6)
    lda #.bits0 | .bits1
    sta $d018
}
%{
function trans2 (a)
    res = {}
    for i,v in ipairs(a) do
        table.insert(res, v + 1)
    end
    return res
end
}%

scr_dest = 0x6000
bitmap_dest = 0x4000

    !section "main", 0x801
    !byte $0b, $08,$01,$00,$9e,str(start)," BY SASQ", $00,$00,$00
start:

    sei
    ldx #$00

-   lda colors,x
    sta $d800,x
    lda colors+$100,x
    sta $d900,x
    lda colors+$200,x
    sta $dA00,x
    lda colors+$2e8,x
    sta $dae8,x 

    !if screen != scr_dest {
        lda screen,x
        sta scr_dest,x
        lda screen+$100,x
        sta scr_dest+$100,x
        lda screen+$200,x
        sta scr_dest+$200,x
        lda screen+$2e8,x
        sta scr_dest+$2E8,x
    }
    inx
    bne -

    VicAdr(0x4000)
    BitmapAndScreen(0x0000, scr_dest-0x4000)

    lda #$18
    sta $d016

    lda #$3b
    sta $d011

    lda #bg_color
    sta $d020
    sta $d021

    jsr $1000
loop
$   lda #100
    cmp $d012
    bne -
    lda #4
    sta $d020
    jsr $1003
    lda #bg_color
    sta $d020
    jmp loop


    koala = load("../data/archmage.koa")
    bitmap = koala[2:0x1f42]
    screen_ram = koala[0x1f42:0x232a]
    color_ram = koala[0x232a:0x2712]
    bg_color = koala[0x2712]

    sid = load("../data/test.sid")[0x7e:]
    !section "music", 0x1000
    !fill sid

    ;sum = add(sid)
    ;!print sum

    t = trans2(sid)
    !print sid[0]," ",t[0]

    !section "colors", *
colors:
    !fill color_ram

    !section "screen", *
screen:
    !fill screen_ram

    !section "koala", 0x4000
    !fill bitmap

