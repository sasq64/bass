

    !section "cart", $8000

    !word start,0
    !byte $C3, $C2, $CD, $38, $30
start:
    sei
$   lda $d012
    sta $d020
    sta $d021
    jmp -

!section "dummy", $9ffc

    !byte 0,0,0,0,0,0,0,0