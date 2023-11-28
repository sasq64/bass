#include <string.h>

// MEMCPY

static void memcpy1(char* dst, const char* src, short len)
{
    while(len--) {
        *dst++ = *src++;
    }
}

static void memcpy2(char* dst, const char* src, short len)
{
    for(int i = 0; i<len; i++) {
        dst[i] = src[i];
    }
}

static void memcpy3(char* dst, const char* src, signed char len)
{
    for(signed char i = len-1; i>=0; i--) {
        dst[i] = src[i];
    }
}

static void memcpy4(char* dst, const char* src, unsigned char len)
{
    for(unsigned char i = len; i>0; i--) {
        (dst-1)[i] = (src-1)[i];
    }
}

static void memcpy5(char* dst, const char* src, unsigned char len)
{
    for(unsigned char i = len-1; i>0; i--) {
        dst[i] = src[i];
    }
    dst[0] = src[0];
}

static void icopy(char* dst, const char* src, int len)
{
    int blocks = len >> 8;
    int i = 0;
    while (1) {
        for(int j=0; j<blocks; j++) {
            (dst + j*256)[i] = (src + j *256)[i];
        }
        i++;
        if (i == 256) break;
    }
    len = len & 0xff;
    if (len == 0) { return; }
    for(unsigned char i = len-1; i>0; i--) {
        (dst + blocks * 256)[i] = (src + blocks * 256)[i];
    }
    (dst + blocks * 256)[0] = (src + blocks * 256)[0];
}

// WRITE

static void write(char const* src, char len)
{
    volatile char* data = (char*)0x1000;
    for(char i = 0; i<len; i++) {
        *data = src[i];
    }
}

// PUT PUXEL

static void put_pixel(unsigned char x, unsigned char y)
{
    volatile unsigned char* vram = (unsigned char*)0x8000;
    vram[(y>>3)*320+y + (x&0xf8)] |= (0x80>>(x&7));
}

extern int len;
extern unsigned char len8;

extern char const* esrc;
extern char* edst;

static char const* src = (char const*)0x2000;
static char* dst = (char*)0x8000;

extern unsigned char x;
extern unsigned char y;

int main()
{
    //memcpy1(dst, src, 50);
    //write(src, 40);
    //put_pixel(10, 10);
    return 0;
}

