    !cpu "4010"

    !org $4000
    bne far
    beq value
    lda ($02),z
    sta [$05],z
    ldq value
    stq value2
    rts

hey:
    bra back


value:
    !long 0x12345678
value2
    !word 0, 0

    !fill 1000, $ea

    !print *

    !test
far:
    ldz #3
    !log "Z={Z}"
    !check Z == 3

    lda #0
    beq hey
    rts
back:
    ldx #1
    rts
    nop

