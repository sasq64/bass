

    lda array+7
    !check A == 2
    rts

array:
    !fill [1,2,3]*3
array_end:

    !assert array_end - array == 9

