	clc
	lda #$90
	adc #$80

	sec

	lda #$50
	sbc #$20
	nop
@req a=0x30,sr=0x01
	sbc #$40
@req a=0xf0,sr=0x80
	adc #$90
@req a=0x80,sr=0x81

	lda #'h'
	sta $d000
	lda #'i'
	sta $d000

  CLC      ; 1 + 1 = 2, returns V = 0
  LDA #$01
  ADC #$01
@req a=2,sr=0
  CLC      ; 1 + -1 = 0, returns V = 0
  LDA #$01
  ADC #$FF
@rea a=0,sr=1
  CLC      ; 127 + 1 = 128, returns V = 1
  LDA #$7F
  ADC #$01
  nop
@req a=0x80sr=0xc0
  CLC      ; -128 + -1 = -129, returns V = 1
  LDA #$80
  ADC #$FF
@req a=0x7f,sr=0x41
  SEC      ; 0 - 1 = -1, returns V = 0
  LDA #$00
  nop
@req sr=0x43
  SBC #$01
@req a=0xff,sr=0x80
  SEC      ; -128 - 1 = -129, returns V = 1
  LDA #$80
@req sr=0x81
  SBC #$01
@req a=0x7f,sr=0x41
  SEC      ; 127 - -1 = 128, returns V = 1
  LDA #$7F
  SBC #$FF
@req a=0x80,sr=0xc0
  rts

