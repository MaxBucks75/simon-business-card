#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ---------- Board Pin Assignments ----------
#define LED_SPI            spi0
#define LED_SPI_SCK_PIN    2
#define LED_SPI_MOSI_PIN   3

// Four Simon buttons (change as needed)
#define BUTTON1_PIN 28 
#define BUTTON2_PIN 5 
#define BUTTON3_PIN 6 
#define BUTTON4_PIN 7 

// ---------- Initialization ----------
void platform_init(void);

// ---------- Time ----------
uint32_t platform_millis(void);
void platform_delay_ms(uint32_t ms);

// ---------- Buttons ----------
void platform_buttons_init(void);
void platform_buttons_pressed(bool *btn1, bool *btn2, bool *btn3, bool *btn4);
bool platform_is_button_down(int idx);

// ---------- SPI for LEDs ----------
void platform_spi_write_bytes(const uint8_t* data, size_t len);

#ifdef __cplusplus
}
#endif

#endif // PLATFORM_H