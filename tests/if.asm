

    !ifndef X {
    X = 3
    }


    !ifdef Y {
        Y = 3
    }

    !test X=9, "hey"
hey:
    clc
    ;!run {: print("X", reg_x()) :}
    txa
    adc #$a0
    ;!run {: print("A", reg_a()) :}
    !check A == $a9
    ldx #$e0
    !log "RTS"
    rts

    !assert X == 3
;    !assert Y != 3
