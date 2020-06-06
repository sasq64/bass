; C64 Example source

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

scr_dest = 0x6000
bitmap_dest = 0x4000

spritePtrs = scr_dest + 1016

    !section "main", 0x801
    !byte $0b, $08,$01,$00,$9e,str(start)," BY SASQ", $00,$00,$00
start:

    sei

    ; Copy colors and screen data
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

    jsr init_sprite

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
    jsr update_sprite
    jmp loop

spriteMem = 0x5f80

init_sprite
    ; Enable sprite 2
    lda #$04 
    sta $d015

    ; Set sprite 2 pointer
    lda #(spriteMem-0x4000)/64
    sta spritePtrs+2

    ; Copy sprite data
    ldx #3*21
$   lda spriteData-1,x
    sta spriteMem-1,x
    dex
    bne -
    rts

update_sprite
    ldx xy
    ldy xy+1
    lda sine,x
    inx
    sta $d004    ; Sprite position x++
    lda sine,y
    iny
    iny
    sta $d005    ; Sprite position y++

    stx xy
    sty xy+1
    rts

xy: !byte 0,0


sine:
    !rept 256 { !byte (sin(i*Math.Pi*2/256)+1) * 100 + 24 }

%{

function setPixel(target, width, x, y)
    offs = width * y + (x>>3) + 1 -- 1-Indexed arrays :(
    if offs >=1 and offs <= #target then
        target[offs] = target[offs] | 1<<(7-(x&7))
    end
end

function circle(target, width, xp, yp, r)
    for y=-r,r, 1 do
        for x=-r,r, 1 do
            if x*x+y*y <= r*r then
                setPixel(target, width, xp+x, yp+y)
            end
        end
    end
    return target
end

}%

circle_sprite = circle(zeroes(3*21), 3, 12, 10, 10)

spriteData
    ;!rept 3*21 { !byte 0xff>>((i%3)+i/8) }

!ifdef BALL {
    !block circle_sprite
} else {
    ; Balloon
    !byte 0,127,0,1,255,192,3,255,224,3,231,224
    !byte 7,217,240,7,223,240,2,217,240,3,231,224
    !byte 3,255,224,3,255,224,2,255,160,1,127,64
    !byte 1,62,64,0,156,128,0,156,128,0,73,0,0,73,0,0
    !byte 62,0,0,62,0,0,62,0,0,28,0
}
    ; Koala Image

    koala = load("../data/oys.koa")
    bitmap = koala[2:0x1f42]
    screen_ram = koala[0x1f42:0x232a]
    color_ram = koala[0x232a:0x2712]
    bg_color = koala[0x2712]

    sid = load("../data/test.sid")[0x7e:]
    !section "music", 0x1000
    !block sid

    !section "colors", *
colors:
    !block color_ram

    !section "screen", *
screen:
    !block screen_ram

    !section "koala", 0x4000
    !block bitmap
