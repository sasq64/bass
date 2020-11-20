

!macro Wait(msec)
{
    ;!if msec < 256 {
    ;    .div = 1
    ;    .n = msec
    ;} else {
        .div = 10
        .n = msec/10
    ;}
    lda #.div
    sta Regs.TimerDiv
    lda Regs.TimerLo
    adc #.n
    sta wait_t
.w  lda Regs.TimerLo
    cmp wait_t
    bne .w
}
!macro CheckQuit()
{
    lda Regs.Key
    cmp #KEY.ESCAPE
    bne .out
    inc Regs.Reset
    jmp  *
.out
}
    wait_t = $11

!macro Border(col)
{
    lda #col<<4
    sta Regs.CFillOut
}

!macro Clear(fg, bg)
{
    lda #(bg << 4) | fg
    sta Regs.CFillIn
}

!enum Regs
{
    ; Window into real console. Default to 40x25
    WinX = $d700
    WinY
    WinW
    WinH

    RealW
    RealH

    TextPtr
    ColorPtr

    ; Write to fill inside of window with given
    ; color
    CFillIn
    
    ; Write to fill border with given color
    CFillOut

    ; Read keyboard fifo. 0 means empty
    Key
    ; Read game controllers, if available. One bit
    ; per key. Not always available
    Joy0
    Joy1

    ; Timer ticks 1000 / TimerDiv per seond
    TimerLo
    TimerHi

    TimerDiv

    ; 0:1 0 = Ascii
    ;     1 = Petscii
    ;     2 = UTF-16
    Flags

    Reset

}

!enum KEY
{
    NO_KEY
    BACKSPACE
    LEFT
    UP
    RIGHT
    DOWN
    PAGEUP
    PAGEDOWN
    HOME
    END
    ENTER
    TAB
    DELETE
    F1
    F2
    F3
    F4
    F5
    F6
    F7
    F8
    ESCAPE = $1b
}

!enum {
    Black
    White
    Red
    Cyan   
    Purple
    Green
    Blue
    Yellow
    Orange
    Brown
    LightRed
    DarkGrey
    Grey
    LightGreen
    LightBlue
    LightGrey
}


