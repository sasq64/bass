
start:

    X = 3

    !if X == 3 {
        nop
        ;!error Illegal
        broken
    } else {
        nop
    }
