

`!rept <number> | <array> '{' <statements> '}'`

1. Parse _statements_ _number_ times. For each parse, set the symbol `i` to
   the iteration number.

2. Parse statements once for each item in _array_. For each iteration, sets `i`
   to the iteration number and `v` to the `i`:th  value in the array.

`!fill <number> | <array> '{' <expression> '}'`

Evaluate _expression_ for each iteration, and write a byte to memory. Essentially
a shorthand for `!rept <arg> { !byte <expression }`.


`!map <symbol> ['=' / ',' ] (<number> | <array>) { <expression> }`

Create a new array, with each item set to the result of evaluating _expression_

