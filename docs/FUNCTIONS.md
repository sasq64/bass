# Functions

## Assembler functions

These functions are used directly from the assembler. They
normally do not have any side effects.

### `sin(f), cos(f), tan(f), asin(f), acos(f), atan(f)`

Trigonometric functions.

### `sqrt(f), min(a,b), max(a,b), pow(f), round(f)`

Other math functions.

### `len(v)`

Returns the length of the given array.

### `load(file)`

Loads the file and returns an array of the content.

### `compare(v0, v1)`

Compares two arrays

```
    !assert compare(data, bytes(0,1,2,3))
```

### `word(v)`

Takes an array of at least 2 elements and returns the 16-bit
value stored.

```
    data = load("file.prg")
    loadAddress = word(data[0:2])
```

### `big_word()`

Like `word()`, but in big endian format.

### `zeroes(n)`

Create an array of _n_ zeroes.

### `bytes(a, b, c, ...)`

Create an array containing the given bytes.

### `to_lower(s)`

Convert the given string to lower case.

### `to_upper(s)`

Convert the given string to upper case.

### `str(n)`

Turns a number into a string

### `num(s)`

Turns a string into a number.

## LUA Functions

These functions can only be called from LUA.

### `reg_a(), reg_x(), reg_y()`

Return the contents of a 6502 register.

### `val = mem_read(adr)`
### `mem_write(adr, val)`

Read or write a value from 6502 memory.

### `set_break_fn(brk, fn)`

Set a lua function to be called when a `brk #n` opcode is executed.
Function is called with _n_ as the single argument.

```
%{
    set_break_fn(5, function(b)
        print("Break executed")
    end)
}%

    brk #5
```

### `map_bank_read(hi_adr, len, fn)`

If the emulator reads memory between `hi_adr<<8` and
`hi_adr<<8 + len*256), call the given function.

```
; Map $f000 - $ffff to funtion that just returns $55
%{
    map_bank_read(0xf0, 16, function(adr)
        return 0x55
    end)
}%
```

### `map_bank_write(hi_adr, len, fn)`

If the emulator writes memory between `hi_adr<<8` and
`hi_adr<<8 + len*256`, call the given function.

### `map_bank_read(hi_adr, len, bank)`

If the emulator reads memory between `hi_adr<<8` and
`hi_adr<<8 + len*256`, map the access to the given _bank_.

A bank is taken as the top byte of a 24-bit address. When this
function is called, the list of sections is searched for a
start address of `bank<<16`, and this section is mapped to
`hi_adr<<8`.

```
    ; Emulate bank switching. Bank is selected by writing
    ; to address $01. Bank is mapped to $a000
%{
    -- Intercept writes to zero page
    map_bank_write(0, 1, function(adr, val)
        -- Always write through
        mem_write(adr, val)
        if adr == 0x01 then
            map_bank_read(0xa0, 1, val)
        end
    end)
}%

    ; Load bank #3 and jsr to it
    lda #3
    sta $01
    jsr $a000
```
