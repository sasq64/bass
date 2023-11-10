  !org $800
buffer = $1000
data = $ff00
!test A=100
fast_write:
    ; x = 255 - a
    ; jump to start + (255 - a) * 6

    sta $4
    lda #255
    sbc $4
    tax

    asl
    bcc +
    inc modify+2
$   sta $4
    asl
    bcc +
    inc modify+2
$   adc $4
    bcc +
    inc modify+2
$   sta modify+1
modify:
    jmp wstart

    !align 256
wstart:
    !rept n=256 {
        lda buffer+n-255,x
        sta data
    }
    rts


!test
write_test:
  lda #100
  jmp fast_write
  rts
