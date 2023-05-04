
W = 2
!include "../asm/test_include.i"

;FIXME: workaround -> "code" label guards from reincluding
!ifndef code {
  !include "../asm/other.i"
}
