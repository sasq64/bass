
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

