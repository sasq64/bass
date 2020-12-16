
    !org $8000
    !test
start:
    ldx #1
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
