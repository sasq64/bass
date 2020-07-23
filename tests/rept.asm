
a:
    !rept 4 { nop }
b:
    !rept 4 { !rept 4 { nop } }
c:
    !section "two", $100
    !rept y,4 { !rept x,4 { !byte x+y*10 } }

    !assert b-a == 4
    !assert c-b == 16
    !assert compare(section.two.data[0:8], bytes(0,1,2,3,10,11,12,13))
