; constant D011VALUE == value in d011
; \1 == Raster row

D011VALUE = 0

!macro DrawRasterColors(row) {
    !if (row & 7) == (D011VALUE & 7) {
    ; 23 cycle macro
    col[row]:
        lda #0                  ;  2
        sta $d020               ;  6
        sta $d021               ; 10
        !rept 5 { nop }
        bit $ea                 ; 23
    } else {
    ;63 cycle macro
    col[row]:
        lda #0                  ;  2
        sta $d020               ;  6
        sta $d021               ; 10
        !rept 25 { nop }
        bit $ea                 ; 63
    }
}

    !section "main", $c000

    !rept 6*8 {
        DrawRasterColors(i + $40)
    }

    !test "bad"
    DrawRasterColors(0)
    rts

    !print tests.bad.cycles
    !assert tests.bad.cycles == 23

    !test "good"
    DrawRasterColors(1)
    rts

    !assert tests.good.cycles == 63

    !test "modify" 
    lda #9
    sta col[$40]+1
    lda #8
    sta col[$41]+1
    rts

    !assert tests.modify.ram[$c001] == 9
    !assert tests.modify.ram[$c010] == 8

