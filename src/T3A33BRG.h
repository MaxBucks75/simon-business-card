#ifndef T3A33BRG_H
#define T3A33BRG_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct { uint8_t r, g, b; } Rgb;

void T3A33BRG_init( void (*spi_write_fn)(const uint8_t* data, size_t len));

void T3A33BRG_write(const Rgb* pixels, int count);

#ifdef __cplusplus
}
#endif
#endif