#include "T3A33BRG.h"
#include <stdlib.h>

static void (*s_spi_write)(const uint8_t* data, size_t len) = 0;

void T3A33BRG_init( void (*spi_write_fn)(const uint8_t* data, size_t len))
{
    s_spi_write = spi_write_fn;
}

void T3A33BRG_write(const Rgb* px, int count)
{
    if (!px || count <= 0 || !s_spi_write) return;

    const size_t start = 4;
    const size_t per   = 4;
    const size_t end   = 4;

    const size_t total = start + count*per + end;

    uint8_t* buf = (uint8_t*)malloc(total);
    if (!buf) return;

    // Start frame: 32 zeros
    for (int i=0;i<4;i++) buf[i]=0x00;

//    uint8_t hdr = 0xE0 | (s_gb & 0x1F);
    uint8_t hdr = 0x9F;

    size_t idx = start;
    for (int i = 0; i < count; i++) {
        uint8_t r = px[i].r;
        uint8_t g = px[i].g;
        uint8_t b = px[i].b;
        buf[idx++] = hdr;
        buf[idx++] = b;
        buf[idx++] = g;
        buf[idx++] = r;
    }

    // End frame: 32 ones
    for (int i=0;i<4;i++) buf[idx++] = 0xFF;

    s_spi_write(buf,total);
    free(buf);
}