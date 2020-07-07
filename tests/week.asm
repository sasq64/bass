
; This routine works for any date from 1900-03-01 to 2155-12-31.
; No range checking is done, so validate input before calling.
;
; I use the formula
;     Weekday = (day + offset[month] + year + year/4 + fudge) mod 7
; where the value of fudge depends on the century.
;
; Input: Y = year (0=1900, 1=1901, ..., 255=2155)
;        X = month (1=Jan, 2=Feb, ..., 12=Dec)
;        A = day (1 to 31)
;
; Output: Weekday in A (0=Sunday, 1=Monday, ..., 6=Saturday)

TMP = 6

WEEKDAY:
         CPX #3          ; Year starts in March to bypass
         BCS MARCH       ; leap year problem
         DEY             ; If Jan or Feb, decrement year
MARCH:   EOR #$7F        ; Invert A so carry works right
         CPY #200        ; Carry will be 1 if 22nd century
         ADC MTAB-1,X    ; A is now day+month offset
         STA TMP
        !require RAM[6]==$7a
         TYA             ; Get the year
         JSR MOD7        ; Do a modulo to prevent overflow
         SBC TMP         ; Combine with day+month
         STA TMP
        !require RAM[6]==$86
         TYA             ; Get the year again
         LSR             ; Divide it by 4
         LSR
        !require A==$1d
         CLC             ; Add it to y+m+d and fall through
         ADC TMP
MOD7:    ADC #7          ; Returns (A+3) modulo 7
         BCC MOD7        ; for A in 0..255
         RTS
MTAB:    !byte 1,5,6,3,1,5,3,0,4,2,6,4      ; Month offsets


    !test {
        LDY #2016-1900
        LDX #10
        LDA #7
        JSR WEEKDAY
        !require A==5
        RTS
    }
