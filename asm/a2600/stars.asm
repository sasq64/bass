;///////////////////////////////////////////////////////
;//	Basic Background Demo by Christian Bogey      //
;//              Friday, April 30, 2004               //
;///////////////////////////////////////////////////////


STARS = 85

        !include "asm/a2600/vcs.h" 

    !section "main", 0x0000
    * = 0xf000
	;SEG CODE
	;ORG $F000	; Start Adress

Start		; Standard init code

	SEI
	CLD	
	LDX #$FF
	TXS
	LDA #0

Clear_Mem
	STA 0,X
	DEX
	BNE Clear_Mem	; Clear mem countdown from $FF to $01 
			; >>>>>>>>> VBLANK Set to 0 Later


	lda	#$0c
	ldx	#$80
	clc
Clear_Mem2
	STA	$7f,X
	adc	#$17
	DEX
	BNE Clear_Mem2	; Clear mem countdown from $FF to $01 
			; >>>>>>>>> VBLANK Set to 0 Later

	

	;lda	#$0C
	;sta	$80
	;lda	#10
	;sta	$f2


New_Frame 
        ; Start of Vertical Sync

        LDA #2 
        STA VSYNC 	; Turn VSYNC On
            
        ; Count 3 Scanlines 

        STA WSYNC 
        STA WSYNC 
        STA WSYNC 

        LDA #0 		; 			// 2 cycles
        STA VSYNC	; Turn VSYNC Off 	// 3 cycles

  	;>>>>>>>>>>>>> Start of Vertival Blank <<<<<<<<<<<<<<<<<<<
  	; Count 37 Scanlines

	LDA  #43	;			// 2 cycles
	STA  TIM64T	;			// 4 cycles
	
	;>>>> Free space for code start 

	ldx	#STARS
Loop3
	lda	$80,x		; OFFSET & DELAY
	clc
	adc	#$10
	tay
	and	#$f0
	cmp	#$80
	bne	ok2
;;;
	dey		
	cpy	#$83
	bne	ok
	ldy	#$8b
ok
	tya
	ora	#$10
	sta	$80,x
	dex
	bne	Loop3
	clc
	bcc	out	
ok2
;;;
	tya
	sta	$80,x
	dex
	bne	Loop3
out
	;>>>> Free space for code end

Wait_VBLANK_End
	LDA INTIM	;			// 4 cycles
	BPL Wait_VBLANK_End	;		// 3 cycles (2)
	
	STA WSYNC	;       		// 3 cycles  Total Amount = 21 cycles
			;			2812-21 = 2791; 2791/64 = 43.60
	LDA #0 
	STA VBLANK 	; Enable TIA Output
			
	;>>>>>>>>>>>>>>>> End of Vertival Blank <<<<<<<<<<<<<<<<<<<
	
	
	;>>>>>>>>>>>>>>>> KERNAL starts here <<<<<<<<<<<<<<<<<<<<<<
	
	; 192 scanlines of picture 
	;LDX #0		; Start colour = 0
        ;LDY #192
	sta	WSYNC

	lda	#0
	sta	COLUBK

	lda	#0b00001110
	sta	COLUP0
	lda	#0b00001010
	sta	COLUP1

	ldx	#STARS
	sta	WSYNC




Loop1
	lda	$80,x
	sta	HMM0
	and	#$f
	tay
	lda	#0
	sta	WSYNC
	sta	ENAM0
delay
	dey
	bne	delay
	sta	RESM0

	sta	WSYNC
	sta	HMOVE
	lda	#2
	sta	ENAM0

	dex
	bne	Loop1




	lda	#0
	sta	ENAM0




	sta	WSYNC
	sta	WSYNC
	sta	WSYNC
	sta	WSYNC

	
	;>>>>>>>>>>>>>>>> KERNAL ends here <<<<<<<<<<<<<<<<<<<<<<<<
	
;////////////// End Of Display ////////////////////////////////////////      	
    
	LDA #0b00000010 		; Disable VIA Output
        STA VBLANK      
        
	; 30 Scanlines Overscan
	LDY #29
Loop2
	STA WSYNC
	DEY
	BNE Loop2
	STA WSYNC	; 29+1 = 30
	
	
        ; Build New Frame 
       	JMP New_Frame 		


sprite0:
	!byte 0
	!byte 0b0111110
	!byte 0b1100000
	!byte 0b1100000
	!byte 0b0111110
	!byte 0b0000011
	!byte 0b1100011
	!byte 0b1100011
	!byte 0b0111110
	!byte 0
sprite1:
	!byte 0
	!byte 0b01111100
	!byte 0b11000110
	!byte 0b11000110
	!byte 0b11111110
	!byte 0b11000110
	!byte 0b11000110
	!byte 0b11000110
	!byte 0b11000110
	!byte 0
sprite2:
	!byte 0b11111111
	!byte 0b11000000
	!byte 0b10000000
	!byte 0b11000000
	!byte 0b01000000
	!byte 0b10000000
	!byte 0b01000000
	!byte 0b11111111
	!byte 0
sprite3:
	!byte 0
	!byte 0b01111100
	!byte 0b11000110
	!byte 0b11000110
	!byte 0b11000110
	!byte 0b11000110
	!byte 0b11000110
	!byte 0b11000111
	!byte 0b01111111
	!byte 0
sprite4:
	!byte 0
	!byte 0b11111111
	!byte 0b10000000
	!byte 0b10000000
	!byte 0b11111111
	!byte 0b10000000
	!byte 0b10000000
	!byte 0b10000000
	!byte 0b11111111
	!byte 0
sprite5:
	!byte 0
	!byte 0b11111111
	!byte 0b00000000
	!byte 0b00000000
	!byte 0b11111111
	!byte 0b10000000
	!byte 0b10000000
	!byte 0b10000000
	!byte 0b10000000
	!byte 0


	;ORG $FFFA 
    !section "vectors", 0xffa

        !word Start 	; NMI - Not used by the 2600 but exists on a 7800
        !word Start     ; RESET 
        !word Start    	; IRQ - Not used by the 2600 but exists on a 7800

        ;END 


