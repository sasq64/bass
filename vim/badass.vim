" orginal from http://codebase64.org/doku.php?id=base:syntax_highligthing_acme_vim by Bitbreaker/Nuance^Metalvotze

syn clear
syn case ignore

syn keyword asm65O2Mnemonics adc and asl bit clc cld cli clv cmp cpx cpy dec dex dey eor inc inx iny lda ldx ldy lsr nop ora pha php pla plp rol ror sbc sec sed sei sta stx sty tax tay tsx txa txs tya

syn match asm6502Index ",\s*[xy]" contains=asm6502Mnemonics

syn keyword asm6502Branch bcc bcs beq bmi bne bpl bvc bvs jmp jsr rts rti brk

syn keyword asm6510Illegal slo rla sre rra sax lax dcp isc anc asr arr sbx dop top jam

syn keyword bassFunction sin cos round bytes str zeroes word load min max


" syn match asmMacroCall		"[a-z_][a-z0-9_]*(.*)"
" syn match asmReferenceCall	"\~[a-z_+-.][a-z0-9_+-]*"
syn match asmComment		";.*" contains=asmTodo
syn keyword asmTodo		contained todo fixme xxx warning danger note notice bug
syn region asmString		start=+"+ skip=+\\"+ end=+"+

syn match asmPseudoop		"![a-z]*"hs=s+1
syn keyword asmPseudoop		else

syn match asmLabel		"^\<[a-z_+-.][a-z0-9_+-]*\>"

syn match decNumber		"\<\d\+\>"
syn match hexNumber		"\(0x\|\$\)\x\+\>"
syn match binNumber		"0b[01]\+\>"
" syn match asmImmediate		"#\(0x\|\$\)\x\+\>"
" syn match asmImmediate		"#\d\+\>"
" syn match asmImmediate		"<\(0x\|\$\)\x\+\>"
" syn match asmImmediate		"<\d\+\>"
" syn match asmImmediate		">\(0x\|\$\)\x\+\>"
" syn match asmImmediate		">\d\+\>"
" syn match asmImmediate		"#<\(0x\|\$\)\x\+\>"
" syn match asmImmediate		"#<\d\+\>"
" syn match asmImmediate		"#>\(0x\|\$\)\x\+\>"
" syn match asmImmediate		"#>\d\+\>"


syn match luaComment		"--.*"

syn keyword luaFunction function end for if then return contained

syn region luaScript start="%{" end="}%" contains=luaFunction,decNumber,hexNumber,binNumber,luaComment

if !exists("did_bass_syntax_inits")
  let did_bass_asm_syntax_inits = 1

  hi link asm6510Illegal	Debug
  hi link asm6502Mnemonics	Statement
  hi link asm6502Index		Operator
  hi link asm6502Branch		Repeat
  hi link asmString		String
  hi link bassFunction  Function
  " hi bassFunction ctermfg=182 guifg=#dfafdf
  hi link asmComment		Comment
  hi link luaComment		Comment
  hi link asmMacroCall		Type
  hi link asmReferenceCall	Debug
  hi link asmPseudoop		Statement
  hi link asmLabel		Macro

  hi link luaFunction Function

  hi link asmTodo		Debug

  hi link asmImmediate		Number

  hi link hexNumber		Number
  hi link binNumber		Number
  hi link decNumber		Number

endif

let b:current_syntax = "badass"
