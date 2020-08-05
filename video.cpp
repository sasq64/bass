#include <cstdint>
// Index: ffff'bbbb'iiii'iiii OR pppp'iiii'iiii'iiii
// f = Fg color index, b = bg color index
// p = Palette offset, i = Tile index

// Single screen mode, 256x256, requires 1024 tiles to fill

enum {

    TileCtrl, // Low 4 bits = plane enable + Tile Mode

    IndexPtrL, // Points to 16-bit indexes
    IndexPtrH,

    // Points to palette offsets per index
    ColorPtrL,
    ColorPtrH,

    // Point to pixels for tiles
    Bitmap0PtrL,
    Bitmap0PtrH,
    Bitmap1PtrL,
    Bitmap1PtrH,
    Bitmap2PtrL,
    Bitmap2PtrH,
    Bitmap3PtrL,
    Bitmap3PtrH,

    // Points to 16-bit colors
    PalettePtrL,
    PalettePtrH,
};

uint8_t vram[65536];

uint8_t ram[65536];


uint8_t* io = &ram[0];

void render_tile_row(int index, int n)
{
    uint8_t planes = io[TileCtrl];
    uint8_t* tilemem0 = &vram[io[Bitmap0PtrL] | io[Bitmap0PtrH]<<8];
    uint8_t* tilemem1 = &vram[io[Bitmap1PtrL] | io[Bitmap1PtrH]<<8];
    uint8_t* tilemem2 = &vram[io[Bitmap2PtrL] | io[Bitmap2PtrH]<<8];
    uint8_t* tilemem3 = &vram[io[Bitmap3PtrL] | io[Bitmap3PtrH]<<8];

    uint8_t row0 = planes & 1 ? tilemem0[index*8+n] : 0;
    uint8_t row1 = planes & 2 ? tilemem1[index*8+n] : 0;
    uint8_t row2 = planes & 4 ? tilemem2[index*8+n] : 0;
    uint8_t row3 = planes & 8 ? tilemem3[index*8+n] : 0;

    pal_ptr = pal_mem + palette_offsets[index];

    ; to 8 4 bit values

    for(int i=0; i<7; i++) {
        int col = ((row0 >> i) & 1) | (((row1 >> i) & 1) << 1) |
                    (((row2 >> i) & 1) << 2) | (((row3 >> i) & 1) << 3);
        *screen++ = palette[col];
    }
}

void render_scan_line(int line)
{
    int row = line & 7; // 0-7
    for(int x = 0; x<256; x+= 8;
        int index = indexmem[x/8] & index_mask;

        render_tile_row(index, row);
     }
}
