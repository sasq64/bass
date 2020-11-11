
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
    BACKSPACE = 1
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
    White
    ; Comment
    Red
    Green
    Blue
    Orange
    Black
    Brown
    LightRead
    DarkGrey
    Grey
    LightGreen
    LightBlue
    LightGrey
    Purple
    Yellow
    Cyan   
}


