
    !org $8000
    !test

start:
    ldx #1
    ldy #45
    lda to_array(256, [i-> sin(i*M_PI*2/256) * 20]),y
    !check A == trunc(sin(45*M_PI*2/256) * 20)
    lda [2,4,8,6],x
    !check A == 4

    lda "POLICE",x
    !check A == 15
    !log "A={A}"

    jmp more
    
    !place
more:
    ldx #4
    lda "POLICE",x
    !log "A={A}"
    !check A == 3
    rts
