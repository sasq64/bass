!org 0x1234
!include "eot_comment.asm"
main_var = included_var+1
start_of_test:
    !assert included_var == 42, "test failed: included_var differs"
    !assert main_var == 43; "test failed: main_var differs"
    sta $c000
