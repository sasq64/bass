
### `!rept`

1. `!rept <count> { <statements...> }`
2. `!rept <array> { <statements...> }`


1. Parse _statements_ _count_ times. For each parse, set the symbol `i` to
   the iteration number.

2. Parse statements once for each item in _array_. For each iteration, sets `i`
   to the iteration number and `v` to the `i`th  value in the array.

### `!fill`

`!fill <number> | <array> { <expression> }`

Evaluate _expression_ for each iteration, and write a byte to memory. Essentially
a shorthand for `!rept <arg> { !byte <expression }`.

### `!map`

`!map <symbol> = <number> | <array> { <expression> }`

Create a new array, with each item set to the result of evaluating _expression_



### `!macro`

`!macro <name>(<args>...) { <statements...> }` 

Create a macro that will insert `<statements>`.

###  `!define`

`!define <name>(<args>...) { <expression> }` 

`<name>` becomes a function that returns `<epression>`.

### `!byte <expression> [,<expression>]...`

Insert bytes into memory

### `!text`

`!text <string> [,<string>]`

Insert characters into memory. Characters are translated using
current translation table.

### `!enum`

`!enum [<name>] { <assignments...> }`

Perform the assignments into a symbol table called `name`, or
directly to the global symbol table if no name is given.