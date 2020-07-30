
    BANK_SELECT = $01
    BANK_SAVE = $02

    ; We emulate bank switching at $a000 by writing bank to $01
    ; We map the zero page write to our funtion
%{
    map_bank_write(0, 1, function(adr, val)
        -- Always write through to zero page
        mem_write(adr, val)
        if adr == 0x01 then
            map_bank_read(0xa0, 1, val)
        end
    end)
}%

    ; Far jump to 24-bit address with bank in top 8 bits
    !macro jsrf(adr) {
        .PC = *
        pha
        ; Jump from unbanked memory
        !if .PC < 0xa000 {
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

    !section "farjump",in="bootbank" {
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


    !section name="code",in="bootbank" {
;game_start:

    ldx #(far_end-far_jsr)
$   lda ram_code,x
    sta far_jsr,x
    dex
    bne -

$   lda $d012
    sta $d020
    jmp -
}

    !section "boot",in="bootbank",start=$bffc {
    !word $e000
}

    !section "banky", 0x18000
    nop
    ; ------------------------

    !section "bank1", 0x01a000

func1:
    !check A == 99
    lda #10
    rts

    !section "bankp", 0x28000
    nop

    !section "bank2", 0x02a000

func2:
    lda #20
    rts

far_jumps:
    lda #99
    jsrf func1
    !check A == 10
    rts

