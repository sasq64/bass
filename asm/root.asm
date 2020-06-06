; Calculates the 8 bit root and 9 bit remainder of a 16 bit unsigned integer in
; Numberl/Numberh. The result is always in the range 0 to 255 and is held in
; Root, the remainder is in the range 0 to 511 and is held in Reml/Remh
;
; partial results are held in templ/temph
;
; This routine is the complement to the integer square program.
;
; Destroys A, X registers.

; variables - must be in RAM

Numberl		= $F0		; number to find square root of low byte
Numberh		= Numberl+1	; number to find square root of high byte
Reml		= $F2		; remainder low byte
Remh		= Reml+1	; remainder high byte
templ		= $F4		; temp partial low byte
temph		= templ+1	; temp partial high byte
Root		= $F6		; square root

  *= $1000

TEST = 23456

	lda #TEST >> 8
	sta Numberh
	lda #TEST & 0xff
	sta Numberl
	jsr	SqRoot
	lda	Root
@req a=153
	rts


	*= $8000		; can be anywhere, ROM or RAM

SqRoot
	LDA	#$00		; clear A
	STA	Reml		; clear remainder low byte
	STA	Remh		; clear remainder high byte
	STA	Root		; clear Root
	LDX	#$08		; 8 pairs of bits to do
Loop
	ASL	Root		; Root = Root * 2

	ASL	Numberl		; shift highest bit of number ..
	ROL	Numberh		;
	ROL	Reml		; .. into remainder
	ROL	Remh		;

	ASL	Numberl		; shift highest bit of number ..
	ROL	Numberh		;
	ROL	Reml		; .. into remainder
	ROL	Remh		;

	LDA	Root		; copy Root ..
	STA	templ		; .. to templ
	LDA	#$00		; clear byte
	STA	temph		; clear temp high byte

	SEC			; +1
	ROL	templ		; temp = temp * 2 + 1
	ROL	temph		;

	LDA	Remh		; get remainder high byte
	CMP	temph		; comapre with partial high byte
	BCC	Next		; skip sub if remainder high byte smaller

	BNE	Subtr		; do sub if <> (must be remainder>partial !)

	LDA	Reml		; get remainder low byte
	CMP	templ		; comapre with partial low byte
	BCC	Next		; skip sub if remainder low byte smaller

				; else remainder>=partial so subtract then
				; and add 1 to root. carry is always set here
Subtr
	LDA	Reml		; get remainder low byte
	SBC	templ		; subtract partial low byte
	STA	Reml		; save remainder low byte
	LDA	Remh		; get remainder high byte
	SBC	temph		; subtract partial high byte
	STA	Remh		; save remainder high byte

	INC	Root		; increment Root
Next
	DEX			; decrement bit pair count
	BNE	Loop		; loop if not all done

	RTS
