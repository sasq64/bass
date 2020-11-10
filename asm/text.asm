
    !section "main", $8000

xpos = $02
ypos = $03

factor1 = $04
factor2 = $05



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
