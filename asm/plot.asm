    !include "vic.inc"
    !include "utils.inc"

    !section "zpage", 2, NoStore=true
    
SCREEN = $6000
COLORS = $5000

    !section "RAM",$801
    !byte $0b,$08,$01,$00,$9e,str(start),$00,$00,$00
start:

    sei

    VicAdr($4000)
    BitmapAndScreen($2000, $1000)

    ; Set hires
    lda #$8
    sta $d016
    lda #$3b
    sta $d011

   ; Clear colors to white on black
   MemSet(COLORS, $10, 1000)

    jsr clear_screen
    jsr plot_sine

    ldx #10
    ldy #10
    jsr fast_plot

    ldx #20
    ldy #20
    jsr fast_plot

    ldx #30
    ldy #30
    jsr fast_plot
$
    jmp -

    !section in="zpage", NoStore=true {
sinx: !byte 0
    }

;!test "plot_sine", $c000

plot_sine:
    lda #0
    sta xcoord
    sta xcoord+1
$
    ldx xcoord
    lda sine, x
    sta ycoord
    jsr plotbit
    inc xcoord
    bne -
    inc xcoord+1
$
    ldx xcoord
    cpx #320-256
    beq .done
    lda sine, x
    sta ycoord
    jsr plotbit ; plotbit
    inc xcoord
    bne -
.done
    rts

!test
clear_screen:
    MemSet(SCREEN, 0, 320*200/8)
    rts

;!test "Verify plot pixel logic"
plot_test:
    jsr clear_screen
    lda #0
    sta xcoord+1

    lda #1
    sta xcoord
    lda #0
    sta ycoord
    jsr plotbit
    !check RAM[SCREEN] == 0x40

    lda #5
    sta xcoord
    jsr plotbit
    !check RAM[SCREEN] == 0x44

    lda #9
    sta ycoord
    jsr plotbit
    !check RAM[SCREEN + 40*8 + 1] == 0x4
    rts


    !section in="zpage", NoStore=true {
        loc:    !word 0
        xcoord: !word 0
        ycoord: !byte 0
        mask:   !byte 0
    }

;!test "Plot single pixel"
plotbit:

		lda	#0
		sta loc

		lda ycoord
		lsr
		lsr
		lsr
		sta loc+1
        ; hi = y/8 : ---76543
		lsr
		ror	loc
		lsr
		ror	loc
        ; low =  43------
        ; a = -----765

		adc loc+1
        ; a = y/32 + y/8

		sta loc+1
        ; loc = Y offset : (y/8) * 320
        ;!run { fmt("{:02x}{:02x} {:02x}", mem_read(3), mem_read(2), reg_a()) }

		lda ycoord
		and #7
		adc loc

        ; loc = (y/8) * 320 + y%8
		sta loc
        ; loc = (y/8) + 320 + y%8 + x&f8

		lda loc+1
		adc xcoord+1
		adc #SCREEN>>8
		sta loc+1
		ldy	#0

		lda xcoord
		and #$f8
		;sta store  ; store = x & f8 ( byte offset)
        tay

        ; mask from bottom 3 bits of X
        lda	xcoord
		and	#7
		tax
        lda offs,x

		ora (loc),y
        ;!run { fmt("{:02x}{:02x} {:02x}", mem_read(3), mem_read(2), reg_a()) }
		sta (loc),y
		rts
offs:   !fill 8, [i -> $80>>i]

        ;!section "sine", $8000
sine:
    !rept 256 { !byte (sin(i*Math.Pi*2/256)+1) * 100 }

target = $20

;xy == coord
!test "fast_plot", X=10, Y=10
fast_plot:

    lda lookup_lo,y
    sta target
    lda lookup_hi,y
    sta target+1

    txa
    and #$f8
    tay
    lda lookup_mask,x

    ora (target),y
    sta (target),y
    
    rts
    !assert tests.fast_plot.ram[$614A] == $20

yoffset = [y -> (y>>3) * 40 * 8 + (y&7) + SCREEN ]

!rept 256 {
    !print yoffset(i)
}

lookup_lo:
    !rept 256 {
        !byte (i>>3) * 40 * 8 + (i&7) + SCREEN 
    }
lookup_hi:
    !rept 256 {
        !byte ((i>>3) * 40 * 8 + (i&7) + SCREEN) >> 8
    }
lookup_mask:
    !rept 256 {
        !byte (1<<(7-(i&7)))
    }


