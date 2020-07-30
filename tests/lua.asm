%{
    function lua_test(n)
        return n + 3
    end
}%


    !section "data", $1000

    !assert lua_test(1) == 4

