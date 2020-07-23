
    !ascii
a = "hey "
b = "you"

    !section "main", $800

start:
    nop
    rts

text:
    !fill a + b
end:

!test "main" {
    jsr start
}

!print a + b

!assert end - text == 7
!assert tests.main.ram[text+4] == 'y'
