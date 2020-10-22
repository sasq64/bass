
    !ascii
a = "hey "
b = "you"

    !section "main", $800

!test "main"
start:
    nop
    rts

text:
    !fill a + b
end:

!print a + b

!assert end - text == 7
!assert tests.main.ram[text+4] == 'y'
