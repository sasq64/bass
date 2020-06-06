
    !section "sprites", $0340
MIB_MEM_SP2

    !section "main", 0x801
    !byte $0b, $08,$01,$00,$9e,str(start),$00,$00,$00
start
    ; Black screen
    lda #0
    sta $d020
    sta $d021
    ; Clear screen
    jsr $e544 

    ; Enable sprite 2
    lda #$04 
    sta $d015

    ; Set sprite 2 pointer
    lda #MIB_MEM_SP2/64
    sta $7f8+2

    ; Copy sprite data
    ldx #3*21
$   lda spr0,x
    sta MIB_MEM_SP2,x
    dex
    bne -

loop
    lda #100
$   cmp $d012
    bne -

    lda #101
$   cmp $d012
    bne -

    lda sine,x
    inx
    sta $d004    ; Sprite position x++
    lda sine,y
    iny
    iny
    sta $d005    ; Sprite position y++

    jmp loop

sine:
    !rept 256 { !byte (sin(i*Math.Pi*2/256)+1) * 100 + 24 }

%{

function setPixel(target, width, x, y)
    offs = width * y + (x>>3) + 1
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

spr0
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
