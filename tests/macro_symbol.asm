
!macro some_macro(v0) { 
.macro_label:
  ; some content 
  lda #v0
  nop
}

main:
    some_macro(1)
other:
    some_macro(2)

    rts
