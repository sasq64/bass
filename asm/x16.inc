
    !section "utils", section.main.end

!macro LoadFile(fname) {
     LDA #fname_end-fname
     LDX #<fname
     LDY #>fname
     jsr kernel_load
 }

kernel_load:
        JSR $FFBD     ; call SETNAM
        LDA #$01
        LDX $BA       ; last used device number
        BNE *+4
        LDX #$08      ; default to device 8

        LDY #$01      ; not $01 means: load to address stored in file
        JSR $FFBA     ; call SETLFS

        LDA #$00      ; $00 means: load to memory (not verify)
        JSR $FFD5     ; call LOAD
        ;BCS vbl    ; if carry set, a load error has happened

        rts
