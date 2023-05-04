

poo = 3

x = poo == 3 ? 1 : 2

!assert x == 1


data:
    !fill 100, [i -> i < 50 ? i : 100-i]


    !test "check"
    lda data + 49
    !check A == 49
    lda data + 52
    !check A == 48
    rts


