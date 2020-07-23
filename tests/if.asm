

    !ifndef X {
    X = 3
    }

    !ifdef Y {
        Y = 3
    }

    !assert X == 3
;    !assert Y != 3
