
    !section "main", $8000

xpos = $02
ypos = $04
ypos_hi = $05

dir = $08

factor1 = $06
factor2 = $07

start_t = $09

text_screen = $400


Key = $d708

!enum Regs
{
    ; Window into real console. Default to 40x25
    WinX = $d700
    WinY
    WinW
    WinH

    RealW
    RealH

    TextPtr
    ColorPtr

    ; Write to fill inside of window with given
    ; color
    CFillIn
    
    ; Write to fill border with given color
    CFillOut

    ; Read keyboard fifo. 0 means empty
    Key
    ; Read game controllers, if available. One bit
    ; per key. Not always available
    Joy0
    Joy1

    ; Timer ticks 1000 / TimerDiv per seond
    TimerLo
    TimerHi

    TimerDiv

    ; 0:1 0 = Ascii
    ;     1 = Petscii
    ;     2 = UTF-16
    Flags

}

!enum {
    Right
    Down
    Left
    Up
}

!enum {
    White
    ; Comment
    Red
    Green
    Blue
    Orange
    Black
    Brown
    LightRead
    DarkGrey
    Grey
    LightGreen
    LightBlue
    LightGrey
    Purple
    Yellow
    Cyan   
}


start:
    jsr init

    lda #LightBlue
    sta Regs.CFillOut
    lda #Blue
    sta Regs.CFillIn

loop:
    clc
    lda Regs.TimerLo
    adc #3
    sta start_t
    jsr read_joy

    jsr move

    jsr draw

$   lda Regs.TimerLo
    cmp start_t
    bne -

    jmp loop

read_joy:
    ldx Regs.Key
    beq .out

    cpx #'a'
    bne +
    lda #-1
$   cpx #'d'
    bne +
    lda #1
$
    clc
    adc dir
    and #3
    sta dir

.out
    rts


try:
    jsr move
    jsr draw
    jsr move
    jsr draw
    lda #1
    sta dir
    jsr move
    jsr draw
    jsr move
    jsr draw
    rts
    
init:
    lda #<text_screen
    sta ypos
    lda #>text_screen
    sta ypos_hi
    lda #Right
    sta xpos
    sta dir
    rts

draw:
    lda #'*'
    ldy xpos
    sta (ypos),y

    rts

    !test "draw"
    jsr init
    jsr draw
    rts


!test "move_left", dir=Left,xpos=3,ypos=0,ypos_hi=$04
!test "move_down", dir=Down,xpos=9,ypos=40*6,ypos_hi=$04
move:
    lda dir
    cmp #Down
    beq down
    cmp #Left
    beq left
    cmp #Up
    beq up

right:
    inc xpos
    jmp done
left:
    dec xpos
    jmp done
down:
    !log "Going down"
    lda ypos
    clc
    adc #40
    sta ypos
    bcc done
    inc ypos_hi
    jmp done
up:
    lda ypos
    sec
    sbc #40
    sta ypos
    bcs done
    dec ypos_hi
done:
    rts

test:

    ldx #0
$
    lda text,x
    beq +
    sta $400+40*9+15,x
    inx
    jmp -
$
    rts

    !print tests.move_left.ram[0:16]
    !print tests.move_down.ram[0:16]

    !assert tests.move_left.ram[dir] == 2
    !assert tests.move_left.ram[xpos] == 2

    !assert tests.move_down.ram[dir] == 1
    !assert tests.move_down.ram[ypos] == 24
    !assert tests.move_down.ram[ypos_hi] == $05

text:
    !ascii
    !text "hello people"
    !byte 0


mul:
    ; factors in factor1 and factor2
    lda #0
    ldx  #$8
    lsr  factor1
$
    bcc  .no_add
    clc
    adc  factor2
.no_add:
    ror
    ror  factor1
    dex
    bne  -
    sta  factor2
    ; done, high result in factor2, low result in factor1
    rts
