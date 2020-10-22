

    !section "main", $8000
    !section "other", $2000



    !section in="main" {
start:
    nop
    }

    !section in="other" {
start2:
    nop
    }

    !section in="main" {
more:
    nop
    }



    !assert start == $8000
    !assert start2 == $2000
    !assert more == $8001


!section in="other"

!section "funky", start=$8000, NoStore=true
{
yo:
    jmp yo
    nop
}

last:
    !fill sections.funky

