
    !org $2000

data:
    !fill [i -> i][$80:$E0]

mysin = [x -> (sin(x*M_PI*2/256)+1)*127]

    xxx = mysin[:100]
    !print xxx

sin:
    !fill mysin[:100]


sin2:
    !fill 100, mysin
end:

    !assert sin2-sin == 100
    !assert end-sin2 == 100


    !test "slice"
    ldx #99
$
    lda sin,x
    tay
    lda sin2,x
    !check A == Y
    dex
    bne -
    rts
