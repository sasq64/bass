

!macro SetFileName(name) {
    lda #.fname_end - .fname
    ldx #<.fname
    ldy #>.fname
    !section "text" {
.fname !fill name
.fname_end
    }
}

    !section "text", section.main.end
    !section "utils", section.text.end

kernel_load:
        jsr $ffbd     ; call SETNAM
        lda #$01
        ;ldx $ba       ; last used device number
        ;bne .skip
        ldx #$08      ; default to device 8
;.skip
        ldy #$01      ; not $01 means: load to address stored in file
        jsr $ffba     ; call SETLFS

        lda #$00      ; $00 means: load to memory (not verify)
        jsr $ffd5     ; call LOAD
        ;bcs VBL    ; if carry set, a load error has happened

        rts

