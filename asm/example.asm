;----------------------------------------------------------
; badass -- C64 Example source
; Koala screen, animated sprite and music
; sasq - 2020
;----------------------------------------------------------
!script "../lua/sid.lua"

; Set the 16k VIC area. Can only point
; to $0000, $4000, $8000 or $c000
!macro VicAdr(adr) {
    !assert (adr & $3fff) == 0
    lda #((~adr)&$ffff)>>14
    sta $dd00
}

; Set the Bitmap and Screen offsets within VIC
!macro BitmapAndScreen(bm_offs, scr_offs) {
    !assert (bm_offs & $dfff) == 0
    !assert (scr_offs & $c3ff) == 0
    .bits0 = (bm_offs>>10)
    .bits1 = (scr_offs>>6)
    lda #.bits0 | .bits1
    sta $d018
}

; Silly short hand macros
!macro ldxy(v) {
    ldx v
    ldy v+1
}

!macro stxy(v) {
    stx v
    sty v+1
}

; Koala offsets
!enum Koala {
    bitmap = $2
    screen = $1f42
    colors = $232a
    bg = $2712
    end = $2713
}

musicLocation = $1000

screenMem = $6000
bitmapMem = $4000
spriteMem = $7000
spritePtrs = screenMem + 1016

    !section "RAM",$801
    !section "main",in="RAM",start=$801
    !section "data",in="RAM"

    !section "code",in="main"
    !byte $0b,$08,$01,$00,$9e,str(start),$00,$00,$00
start:

    sei

    ; Copy colors and screen data
    ldx #$00
$   lda colors,x
    sta $d800,x
    lda colors+$100,x
    sta $d900,x
    lda colors+$200,x
    sta $da00,x
    lda colors+$2e8,x
    sta $dae8,x 

    !if screen != screenMem {
        lda screen,x
        sta screenMem,x
        lda screen+$100,x
        sta screenMem+$100,x
        lda screen+$200,x
        sta screenMem+$200,x
        lda screen+$2e8,x
        sta screenMem+$2E8,x
    }
    inx
    bne -

    VicAdr($4000)
    BitmapAndScreen(bitmapMem-$4000, screenMem-$4000)

    jsr init_sprite

    lda #$18
    sta $d016

    lda #$3b
    sta $d011

    lda #bg_color
    sta $d020
    sta $d021

    jsr $1000

$   lda #100
    cmp $d012
    bne -
    lda #4
    sta $d020
    jsr $1003
    lda #bg_color
    sta $d020
    jsr update_sprite
    jmp -

test_func:
    rts

!test "my_test" {
    jsr test_func
}

init_sprite
    ; Enable sprite 2
    lda #$04
    sta $d015

    ; Set sprite 2 pointer
    lda #(spriteMem-$4000)/64
    sta spritePtrs+2

    ; Copy sprite data
    !rept 8 {
        ldx #3*21
    $:  lda sprite[i]-1,x
        sta spriteMem-1+i*64,x
        dex
        bne -
    }
    rts

update_sprite
    ldxy xy
    lda sine_xy,x
    inx
    sta $d004    ; Sprite position x++
    lda sine_xy,y
    iny
    iny
    sta $d005    ; Sprite position y++

    stxy xy

    lda sine_frame,x
    adc #(spriteMem-$4000)/64

    sta spritePtrs+2
    rts

    !section in="data" {
xy: !byte 0,0
    }

sine_xy:
    !rept 256 { !byte (sin(i*Math.Pi*2/256)+1) * 100 + 24 }

sine_frame:
    !rept 256 { !byte (sin(i*Math.Pi*2/96)+1) * 3.5 }

%{

function setPixel(target, width, x, y)
    offs = width * y + (x>>3) + 1 -- 1-Indexed arrays :(
    if offs >=1 and offs <= #target then
        target[offs] = target[offs] | 1<<(7-(x&7))
    end
end

function circle(target, width, xp, yp, radius)
    r = math.floor(radius + 0.5)
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

spriteData:
    !rept 8 {
    sprite[i]:
        !fill circle(zeroes(3*21), 3, 12, 10, i + 3)
        !byte $ff
    }

    ; Koala Image

    koala = load("../data/oys.koa")
    bitmap = koala[Koala.bitmap : Koala.screen]
    screen_ram = koala[Koala.screen : Koala.colors]
    color_ram = koala[Koala.colors : Koala.bg]
    bg_color = koala[Koala.bg]

    ; Music

    !define swap(x) { (x>>8) | (x<<8) }

    sid = load("../data/test.sid")
    music_init = swap(word(sid[$a:$c]))
    music_play = swap(word(sid[$c:$e]))

    !assert music_init == musicLocation

    music_data = sid[$7e:]

    ; Data layout

    !section "music", musicLocation
    !fill music_data

%{
    map_bank_write(0xd4, 1, function(adr, val)
        print("SID", adr - 0xd400, val)
    end)
}%

!test "music_init" {
    jsr $1000
}


!test "music_play" {
    jsr $1003
}

colors:
    !fill color_ram

screen:
    !fill screen_ram

    !section "koala", bitmapMem
    !fill bitmap
