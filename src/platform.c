#include "platform.h"

#include "pico/stdlib.h"
#include "hardware/spi.h"

// ------------------ Time ------------------

uint32_t platform_millis(void) {
    return to_ms_since_boot(get_absolute_time());
}

void platform_delay_ms(uint32_t ms) {
    sleep_ms(ms);
}

// ------------------ Buttons ------------------

void platform_buttons_init(void) {
    const int pins[4] = { BUTTON1_PIN, BUTTON2_PIN, BUTTON3_PIN, BUTTON4_PIN };

    for (int i = 0; i < 4; i++) {
        gpio_init(pins[i]);
        gpio_set_dir(pins[i], GPIO_IN);
    }
}

void platform_buttons_pressed(bool *btn1, bool *btn2, bool *btn3, bool *btn4)
{
    const int pins[4] = { BUTTON1_PIN, BUTTON2_PIN, BUTTON3_PIN, BUTTON4_PIN };
    static bool last_button_state[4] = {false, false, false, false};

    bool btn1_current = gpio_get(pins[0]);
    bool btn2_current = gpio_get(pins[1]);
    bool btn3_current = gpio_get(pins[2]);
    bool btn4_current = gpio_get(pins[3]);

    // Check for falling edge of each button (pressed state)
    if ((btn1_current == 0) && (last_button_state[0] == 1)) {
        *btn1 = true;
    }
    if ((btn2_current == 0) && (last_button_state[1] == 1)) {
        *btn2 = true;
    }
    if ((btn3_current == 0) && (last_button_state[2] == 1)) {
        *btn3 = true;
    }
    if ((btn4_current == 0) && (last_button_state[3] == 1)) {
        *btn4 = true;
    }

    // Update last button states
    last_button_state[0] = btn1_current;
    last_button_state[1] = btn2_current;
    last_button_state[2] = btn3_current;
    last_button_state[3] = btn4_current;
}

// ------------------ SPI for LEDs ------------------

void platform_spi_write_bytes(const uint8_t* data, size_t len) {
    spi_write_blocking(LED_SPI, data, len);
}

// ------------------ Global Platform Init ------------------

void platform_init(void) {
    stdio_init_all();

    // Init buttons
    platform_buttons_init();

    // Init SPI for the LED chain
    spi_init(LED_SPI, 1000000); // 1 MHz (T3A33BRG supports up to ~15 MHz)

    spi_set_format(
        LED_SPI,
        8,              // bits
        SPI_CPOL_0,     // clock idle low
        SPI_CPHA_0,     // data latched rising edge
        SPI_MSB_FIRST
    );

    gpio_set_function(LED_SPI_SCK_PIN,  GPIO_FUNC_SPI);
    gpio_set_function(LED_SPI_MOSI_PIN, GPIO_FUNC_SPI);
}

bool platform_is_button_down(int idx)
{
    const int pins[4] = { BUTTON1_PIN, BUTTON2_PIN, BUTTON3_PIN, BUTTON4_PIN };
    if (idx < 0 || idx >= 4) return false; // Invalid index
    return gpio_get(pins[idx]) == 0; // Active low
}