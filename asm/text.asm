
    !section "main", $8000

xpos = $02
ypos = $04
ypos_hi = $05

dir = $08

factor1 = $06
factor2 = $07

text_screen = $0400

lda

    lda <text_screen
    sta ypos
    lda >text_screen
    sta ypos_hi
    lda #0
    sta xpos

loop:

    ldx xpos
    sta (ypos),x


!test "move_left", dir=2,xpos=3,ypos=0,ypos_hi=$04
!test "move_down", dir=1,xpos=3,ypos=0,ypos_hi=$04
move:
    lda dir
    cmp #1
    beq down
    cmp #2
    beq left
    cmp #3
    beq up

right:
    inc xpos
    jmp done
left:
    dec xpos
    jmp done
down:
    !log "Going down"
    lda ypos+1
    adc #40
    sta ypos+1
    bcc done
    inc ypos
    jmp done
up:
    lda ypos+1
    sec
    sbc #40
    sta ypos+1
    bcs done
    dec ypos
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
