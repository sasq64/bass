#include <stdint.h>

unsigned char* vram = (unsigned char*)0x6000;

extern void put_pixel(word x, byte y)
{
    word o = y/8;
    o = o * 320;// + x&0xfff8;
    o = o + y&7;
    o = o + x & 0xfff8; 
    *((unsigned char*)0x6000 + o) |= (0x80>>(x&7));
}

int main()
{
    for (uint8_t i=0; i<100; i++) {
        put_pixel(i, i<<2);
    }
    return 0;
}
