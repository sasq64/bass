
    hey = $1234

    hi = >hey
    lo = <hey

    !assert lo == $34
    !assert hi == $12

    ; Test Left-To-Right associativity of < and >
    hi2 = >hey + $70 * 2
    lo2 = <hey + $100


    !assert hi2 == $13
    !assert lo2 == $34


    a = 103 % 10;

    !assert a == 3

    z = %101
    y = 0b101
    !print z
    !print y
    b = z + y

    !print %101 % %10

    !assert b == 10


    !assert %110 + %1001 == %1111


