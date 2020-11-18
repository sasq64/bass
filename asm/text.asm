    !include "text.inc"

    !section "main", $8000

xpos = $02
ypos = $04
ypos_hi = $05

dir = $08



factor1 = $06
factor2 = $07

start_t = $09

text_screen = $400

!enum {
    Right
    Down
    Left
    Up
}


start:
    jsr init

    lda #Grey
    sta Regs.CFillOut
    lda #Blue
    sta Regs.CFillIn

loop:
    clc
    lda Regs.TimerLo
    adc #5
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

    cpx #KEY.ESCAPE
    bne +
    inc Regs.Reset
    jmp  *

$   cpx #KEY.LEFT
    bne +
    lda #-1
$   cpx #KEY.RIGHT
    bne +
    lda #1
$
    clc
    adc dir
    and #3
    sta dir

.out
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
