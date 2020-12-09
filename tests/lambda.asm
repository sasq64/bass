
mysin = [ x, amp, size ->
         (sin(x * Math.Pi * 2 / size) * 0.5 + 0.5) * amp ]
sin:
    !fill 100 { mysin(i, 255, 100) } 
    
    !assert mysin(0, 100, 100) == 50