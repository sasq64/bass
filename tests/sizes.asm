

start:
    lda.w $05
one:
    sta.b target
two:
    rts

!assert one-start == 3

!assert two - one == 2

target = $4

