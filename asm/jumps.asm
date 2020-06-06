START
	jmp LATER

Sub:	clc
		bcs START
		rts
	
LATER
	jsr	Sub
	lda #8
	rts

