
## `!section`

1. `!section <name>, <start> [, <flags>] [ { <statements...> } ]`
2. `!section <name> [ { <statements...> } ]`
3. `!section { <statements...> }`

Create or activate a section. All code and data must be placed into a section. The sections are gathered at the end and written to the output file.

1. Create a new section beginning at address _start_. If a block is provided, all statements goes into the section, otherwise the section is active until the next section directive.
2. Open up a previous section. With a block the old section is restored after the block was parsed.
3. Group statements together in the current section.

## `!rept`

1. `!rept <count>[,<ivar>] { <statements...> }`
2. `!rept <array>[,<ivar>[,<vvar>]]  { <statements...> }`

Repeat a block of statements a specific number of times.

1. Parse _statements_ _count_ times. For each parse, set the symbol `i` to the iteration number.

2. Parse statements once for each item in _array_. For each iteration, sets `i` to the iteration number and `v` to the `i`th  value in the array.

## `!fill`

1. `!fill <number> | <array> { <expression> }`

Put data into memory,

1. Evaluate _expression_ for each iteration, and write a byte to memory. Essentially a shorthand for
`!rept <arg> { !byte <expression }`.

## `!map`

1. `!map <symbol> = <size> { <expression> }`
2. `!map <symbol> = <array> { <expression> }`

Create a new array from repeating the given expresion.

1. Create a new array with given _size_, where each item set to the result of evaluating _expression_.
2. Transform _array_ into a new array by evaluating _expression_ for each item.

## `!macro`

1. `!macro <name>(<args>...) { <statements...> }` 

Create a macro. A macro can be called at a later time, at which point its contents will be inserted into the assembly for parsing.

1. Create a macro that will insert `<statements>`.

##  `!define`

1. `!define <name>(<args>...) { <expression> }` 

[//]:<>

1. `<name>` becomes a function that returns `<epression>`.

## `!byte`

1. `!byte <expression> [,<expression>]...`

Insert bytes into memory.

## `!word`

1. `!word <expression> [,<expression>]...`

Insert 16bit words into memory.

## `!byte3`

1. `!byte3 <expression> [,<expression>]...`

Insert 24bit words into memory. Useful for C64 sprites.

## `!text`

`!text <string> [,<string>]`

Insert characters into memory. Characters are translated using
current translation table.

## `!enum`

`!enum [<name>] { <assignments...> }`

Perform the assignments into a symbol table called `name`, or
directly to the global symbol table if no name is given.