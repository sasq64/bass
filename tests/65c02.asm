

    !section "main",$1000
start:
    !log "Start"
    nop
    bra +
    nop
$
    lda #<table
    sta $02
    lda #>table
    sta $03

    lda #<(table+1)
    sta $06
    lda #>(table+1)
    sta $07

    lda ($02)
    !check A == 1
    sec
    adc ($06)
    !check A == 4

    lda $04
    !check A == $55
    ldx #1
    bbr $04:1,.s0
    inx
.s0
    bbr $04:0,+
    inx
$
    !check X == 2

    lda #34
    bbs $04:6,+
    lda #12
$
    !check A == 34


    ldx table+1
    lda #$01
    tsb table+1
    beq +
    ldx #0
$   ldy table+1
    !check X == 2
    !check Y == 3

    lda #$02
    tsb table+1
    bne +
    ldx #0  
$
    !check X == 2

    rts

    !section "table",$4000
table:
    !byte 1,2,3

    !section "zpage", $0

    !byte 0,0,0,0
    !byte $55,$aa


!test "test" {
    !log "Testing"
    jsr start
}
