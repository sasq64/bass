## Intro to the _bass_ (badass) 6502 assembler

### Part 1
_bass_ is a new 6502 assembler. It is designed to be powerful (like kickassembler)
but with a less complex syntax, and more _unified_, _functional_ design.


Let's start with a minimal C64 program, a 3 opcode routine to make a rainbow border:

```cpp
    !org 0xc000
loop:
    lda 0xd012
    sta 0xd020
    jmp loop
```

(note that I used the `0x` prefixes for hilighting purposes. I personally prefer `$`,
which of course works too).

You should be able to load this into a C64 emulator and start it with `sys 49152`.

But let's add a _basic start_ snippet so we can just run the prg instead:

```cpp
    !section "main", 0x801
    !byte 0x0b, 0x08,0x01,0x00,0x9e,str(start)," BY SASQ", 0x00,0x00,0x00
start:
```

What happens here ? Well the `!byte` meta command accepts both numbers
and strings, and for strings it inserts each character one by one.

The `str()` function takes a value and converts it to a string. So this
is how we insert the correct sys adress into the basic, regardless of
where that `start:` label is placed.

Let's add some more code;
    
```cpp
    !section "main", 0x801
    !byte 0x0b, 0x08,0x01,0x00,0x9e,str(start)," BY SASQ", 0x00,0x00,0x00
start:
    jsr 0x1000
.loop
    lda 0xd012
    cmp #100
    bne .loop
    lda #3
    sta 0xd020
    jsr 0x1003
    lda #0
    sta 0xd020
    jmp .loop

    !section "music", 0x1000
    !incbin "music.raw"
```

This is a short program that wait for a certain scan line, calls a music routine,
and loops. The music is expected to reside in the file "music.raw" and must
be placed at address 0x1000.

But what if music is a PRG with 2 bytes load address first. Or a sid file ?

The functional approach of bass lets us manipulate not only numeric data, but also
byte arrays. So to strip out the 2 first byte of a file and place it in memory we
can do:

```cpp
    data = load("music.bin")
    !block data[2:]
```

A few new things here, the `data` symbol now refers to a byte array, and we use
the `!block` meta command to insert it into memory.
The `[2:]` part is called _slicing_. To extract a range from a byte array you use
it like: `data[start:end]` (where start is inclusive and end exclusive).

So what about the load address, can we use it? Sure;

```cpp
    loadAdr = word(data[0:2])
    !section "music", loadAdr
music:
    !block data[2:]
```

As you may have figured out, the `word()` function decodes a little endian 16-bit word from an array.

Except what if the load address is not compatible with our layout? Better
to make sure it is where we want it:

```cpp
    musicStart = 0x1000
    !assert word(data[0:2]) == musicStart
```

Lets put it all together:

```cpp
    musicStart = 0x1000

    !section "main", 0x801
    !byte 0x0b, 0x08,0x01,0x00,0x9e,str(start)," BY SASQ", 0x00,0x00,0x00
start:
    jsr musicStart
.loop
    lda 0xd012
    cmp #100
    bne .loop
    lda #3
    sta 0xd020
    jsr musicStart+3
    lda #0
    sta 0xd020
    jmp .loop

    data = load("music.prg")
    !assert  word(data[0:2]) == musicStart

    !section "music", musicStart
    !block data[2:]
```

### Part 2

Let's try to add some graphics. First we add a couple of macros to make it
easier to work with the VIC:

```cpp
; Set VIC base address
!macro VicAdr(adr) {
    !assert (adr & 0x3fff) == 0
    lda #((~adr)&0xffff)>>14
    sta 0xdd00
}

; Set Bitmap and screen offsets
!macro BitmapAndScreen(bm_offs, scr_offs) {
    !assert (bm_offs & 0xdfff) == 0
    !assert (scr_offs & 0xc3ff) == 0
    .bits0 = (bm_offs>>10)
    .bits1 = (scr_offs>>6)
    lda #.bits0 | .bits1
    sta 0xd018
}
```

These macros transforms an address and two offsets into the right bits.
They also make sure that you only specify a legal address/offset.

```cpp
    VicAdr(0x4000)
    BitmapAndScreen(0x2000, 0x000)
```

This means that we need to locate our bitmap data at address `0x6000` and our
screen data at `0x4000`.
Let's import a koala picture:

```cpp
    koala = load("image.koa")
    bitmap = koala[2:0x1f42]
    screen_ram = koala[0x1f42:0x232a]
    color_ram = koala[0x232a:0x2712]
    bg_color = koala[0x2712]
            
    !section "colors", *
colors:
    !block color_ram

    !section "screen", *
screen:
    !block screen_ram

    !section "koala", 0x6000
    !block bitmap
```

Here we use array slicing again to extract all the parts.

We also use the `*` symbol which means the current program counter. Using
it in a `!section` command only for informative purposes.

The bitmap we placed directly at `0x6000`, but the screen and color
data needs to be copied.

```cpp

    scrTarget = 0x4000

    lda #$18
    sta $d016

    lda #$3b
    sta $d011

    lda #bg_color
    sta $d020
    sta $d021

$   lda colors,x
    sta 0xd800,x
    lda colors+0x100,x
    sta 0xd900,x
    lda colors+0x200,x
    sta 0xdA00,x
    lda colors+0x2e8,x
    sta 0xdae8,x 

    !if screen != screenTarget {
        lda screen,x
        sta scrTarget,x
        lda screen+0x100,x
        sta scrTarget+0x100,x
        lda screen+0x200,x
        sta scrTarget+0x200,x
        lda screen+0x2e8,x
        sta scrTarget+0x2E8,x
    }
    inx
    bne -
```

We use an `!if` statement here to avoid the copy just in case we do relocate
the screen data to the correct position at load time.

### Part 3

Let's add a sprite!.

```cpp
spritePtrs = screenTarget + 1016
spriteMem = 0x5f80

    ; Enable sprite 2
    lda #$04 
    sta $d015

    ; Set sprite 2 pointer
    lda #(spriteMem-0x4000)/64
    sta spritePtrs+2

    ; Copy sprite data
    ldx #3*21
$   lda spriteData,x
    sta spriteMem,x
    dex
    bne -

spriteData:
    !rept 3*24 { !byte 0xff }

```

This should create a complete filled box and display it. But of course we want
to generate something more nice looking. For this we turn to lua scripting;

```lua
%{

function setPixel(target, width, x, y)
    offs = width * y + (x>>3) + 1 -- 1-Indexed arrays :(
    if offs >=1 and offs <= #target then
        target[offs] = target[offs] | 1<<(7-(x&7))
    end
end

function circle(target, width, xp, yp, r)
    for y=-r,r, 1 do
        for x=-r,r, 1 do
            if x*x+y*y <= r*r then
                setPixel(target, width, xp+x, yp+y)
            end
        end
    end
    return target
end

}%
```

Now we have access to a `circle()` function that renders a circle into a vector of bits.


```cpp
    circle_sprite = circle(zeroes(3*21), 3, 12, 10, 10)
spriteData:
    !block circle_sprite
```

Here is some code to make the sprite move in a sine wave:

```cpp
update_sprite
    ldx xy
    ldy xy+1
    inc xy
    inc xy+1
    inc xy+1

    lda sine,x
    sta $d004
    lda sine,y
    sta $d005

    rts

xy: !byte 0,0

sine:
    !rept 256 { !byte (sin(i*Math.Pi*2/256)+1) * 100 + 24 }
```

The full example can be found in [asm/example.asm](asm/example.asm)
