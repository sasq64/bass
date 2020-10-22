
    BANK_SELECT = $01
    BANK_SAVE = $02

    ; We map the zero page write to bank switching
%{
    map_bank_write(0, 1, function(adr, val)
        -- Always write through to zero page
        mem_write(adr, val)
        if adr == 0x01 then
            print("BANK", val)
            -- Map a000-c000 -> bank 'val'
            map_bank_read(0xa0, 32, val)
            -- Map 8000-a000 -> bank 'val'
            map_bank_read(0x80, 32, val)
        end
    end)
}%

    ; Far jump to 24-bit address with bank in top 8 bits
    !macro jsrf(adr) {
        .PC = *
        pha
        !if .PC < 0x8000 {
            ; Jump from unbanked memory
            lda #adr>>16
            sta BANK_SELECT
            jsr adr & 0xffff
            pla
        } else {
            ; Jump from banked memory, we need
            ; to modify a trampoline in RAM and
            ; jump from there
            lda BANK_SELECT
            sta BANK_SAVE
            lda #<adr
            sta jump_adr+1
            lda #>adr
            sta jump_adr+2
            lda #adr>>16
            jmp far_jsr
        }
    }

    !section "bootbank", 0xa000

    !section "trampoline",in="bootbank" {
ram_code:
    * = $200
far_jsr:
    sta BANK_SELECT
    pla
jump_adr:
    jsr $ffff
    pha
    lda BANK_SAVE
    sta BANK_SELECT
    pla
    rts
far_end:
}


    !section "code",in="bootbank" {
game_start:

    ldx #(far_end-far_jsr)
$   dex
    lda ram_code,x
    sta far_jsr,x
    bne -
    rts

;$   ;lda $d012
    ;sta $d020
    ;jmp -
}

    !section "boot",in="bootbank",start=$bffc {
    !word $e000
}

    !section "banky", 0x18000
    ldy #$10
    rts
    ; ------------------------

    !section "bank1", 0x01a000

func1:
    !check A == 99
    lda #10
    rts

    !section "bankp", 0x28000
    ldy #$20
    rts

    !section "bank2", 0x02a000

func2:
    lda #20
    rts

far_jumps:
    lda #99
    jsrf func1
    !check A == 10
    jsr $8000
    !check Y == $20
    rts

    !section "test", $2000
    !test "test"
    jsr game_start
    lda #2
    sta $01
    jsr far_jumps
    lda #1
    sta $01
    jsr $8000
    !check Y == $10
    rts
