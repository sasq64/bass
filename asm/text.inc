

!macro Wait(frames)
{
    ldx #frames
.l  lda Regs.IrqReq
    and #1
    beq .l
    sta Regs.IrqReq
    dex
    bne .l

}
!macro CheckQuit()
{
    lda Regs.Key
    cmp #KEY.ESCAPE
    bne .out
    inc Regs.Control
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

    ScrollX
    ScrollY

    ; Read keyboard fifo. 0 means empty
    Key

    Freq

    ; Timer ticks 1000 / TimerDiv per second
    TimerLo
    TimerHi

    Control
    Charset

    IrqEna
    IrqReq

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


