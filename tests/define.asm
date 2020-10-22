
toRadians = [a -> a / 360 * 2 * Math.Pi]


!section "main", 0x1234


Sin: !rept $100 {
    !print 64+64*sin(toRadians((i+0.5)*360.0/128.0))
    !byte 64+64*sin(toRadians((i+0.5)*360.0/128.0))
}
