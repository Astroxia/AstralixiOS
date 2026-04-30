//
//  PicoCalc LCD display driver - Ported to Luckfox Lyra (RK3506)
//
//  This driver interfaces with the ST7789P LCD controller.
//  Ported from Pico SDK to RK3506 bare-metal environment.
//
//  NOTE: This is a TEMPLATE - you need to configure the following:
//  1. Include the correct RK3506 SPI and GPIO headers
//  2. Update pin definitions for your hardware
//  3. Adjust SPI and GPIO function calls to match your SDK
//

#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>

// TODO: Replace these with your RK3506 SDK headers
// #include "rk3506_spi.h"
// #include "rk3506_gpio.h"
// #include "rk3506_timer.h"

#include "lcd.h"

// TODO: Define your delay functions based on RK3506 SDK
// These are placeholders - replace with actual implementations
static inline void sleep_us(uint32_t us) {
    // Implement microsecond delay
    volatile uint32_t count = us * 100; // Adjust multiplier based on CPU speed
    while(count--);
}

static inline void sleep_ms(uint32_t ms) {
    sleep_us(ms * 1000);
}

// TODO: Implement interrupt disable/enable for your platform
static uint32_t irq_state;

static void lcd_disable_interrupts() {
    // For ARM Cortex-A7, use CPSR manipulation or your SDK's interrupt functions
    // Example: irq_state = __get_CPSR(); __disable_irq();
    irq_state = 0; // Placeholder
}

static void lcd_enable_interrupts() {
    // Restore interrupts
    // Example: __set_CPSR(irq_state);
}

static bool lcd_initialised = false;

static uint16_t lcd_scroll_top = 0;
static uint16_t lcd_memory_scroll_height = FRAME_HEIGHT;
static uint16_t lcd_scroll_bottom = 0;
static uint16_t lcd_y_offset = 0;

static uint16_t foreground = 0xFFFF;
static uint16_t background = 0x0000;

static bool underscore = false;
static bool reverse = false;
static bool bold = false;

const font_t *font = &font_8x10;
static uint16_t char_buffer[8 * GLYPH_HEIGHT] __attribute__((aligned(4)));
static uint16_t line_buffer[WIDTH * GLYPH_HEIGHT] __attribute__((aligned(4)));

static uint8_t cursor_column = 0;
static uint8_t cursor_row = 0;
static bool cursor_enabled = true;
static bool cursor_visible = false;

//
// Character attributes
//

void lcd_set_reverse(bool reverse_on) {
    if ((reverse && !reverse_on) || (!reverse && reverse_on)) {
        uint16_t temp = foreground;
        foreground = background;
        background = temp;
    }
    reverse = reverse_on;
}

void lcd_set_underscore(bool underscore_on) {
    underscore = underscore_on;
}

void lcd_set_bold(bool bold_on) {
    bold = bold_on;
}

void lcd_set_font(const font_t *new_font) {
    font = new_font;
}

uint8_t lcd_get_columns(void) {
    return WIDTH / font->width;
}

uint8_t lcd_get_glyph_width(void) {
    return font->width;
}

void lcd_set_foreground(uint16_t colour) {
    if (reverse) {
        background = colour;
    } else {
        foreground = colour;
    }
}

void lcd_set_background(uint16_t colour) {
    if (reverse) {
        foreground = colour;
    } else {
        background = colour;
    }
}

//
// Low-level SPI functions - TODO: Implement for RK3506
//

// TODO: Replace these with your RK3506 GPIO functions
static inline void gpio_set_value(uint32_t pin, uint8_t value) {
    // Example: *(volatile uint32_t*)(GPIO_BASE + offset) = value;
    // Or use your SDK's GPIO function
}

// TODO: Replace with your RK3506 SPI write function
static inline void spi_write_blocking(uint8_t *data, size_t len) {
    // Write data to SPI peripheral
    // Example:
    // for(size_t i = 0; i < len; i++) {
    //     while(!(SPI1->SR & SPI_SR_TXE)); // Wait for TX empty
    //     SPI1->DR = data[i];
    // }
}

static inline void spi_write16_blocking(uint16_t *data, size_t len) {
    // Write 16-bit data to SPI peripheral
}

static inline void spi_set_format_8bit(void) {
    // Configure SPI for 8-bit transfers
}

static inline void spi_set_format_16bit(void) {
    // Configure SPI for 16-bit transfers
}

void lcd_write_cmd(uint8_t cmd) {
    gpio_set_value(LCD_DCX, 0); // Command
    gpio_set_value(LCD_CSX, 0);
    spi_write_blocking(&cmd, 1);
    gpio_set_value(LCD_CSX, 1);
}

void lcd_write_data(uint8_t len, ...) {
    va_list args;
    va_start(args, len);
    gpio_set_value(LCD_DCX, 1); // Data
    gpio_set_value(LCD_CSX, 0);
    for (uint8_t i = 0; i < len; i++) {
        uint8_t data = va_arg(args, int);
        spi_write_blocking(&data, 1);
    }
    gpio_set_value(LCD_CSX, 1);
    va_end(args);
}

void lcd_write16_data(uint8_t len, ...) {
    va_list args;
    
    spi_set_format_16bit();
    
    va_start(args, len);
    gpio_set_value(LCD_DCX, 1); // Data
    gpio_set_value(LCD_CSX, 0);
    for (uint8_t i = 0; i < len; i++) {
        uint16_t data = va_arg(args, int);
        spi_write16_blocking(&data, 1);
    }
    gpio_set_value(LCD_CSX, 1);
    va_end(args);
    
    spi_set_format_8bit();
}

void lcd_write16_buf(const uint16_t *buffer, size_t len) {
    spi_set_format_16bit();
    
    gpio_set_value(LCD_DCX, 1); // Data
    gpio_set_value(LCD_CSX, 0);
    spi_write16_blocking((uint16_t *)buffer, len);
    gpio_set_value(LCD_CSX, 1);
    
    spi_set_format_8bit();
}

//
// ST7789P LCD controller functions
//

static void lcd_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    lcd_write_cmd(LCD_CMD_CASET);
    lcd_write_data(4,
                   UPPER8(x0), LOWER8(x0),
                   UPPER8(x1), LOWER8(x1));

    lcd_write_cmd(LCD_CMD_RASET);
    lcd_write_data(4,
                   UPPER8(y0), LOWER8(y0),
                   UPPER8(y1), LOWER8(y1));

    lcd_write_cmd(LCD_CMD_RAMWR);
}

void lcd_blit(const uint16_t *pixels, uint16_t x, uint16_t y, uint16_t width, uint16_t height) {
    lcd_disable_interrupts();
    if (y >= lcd_scroll_top && y < HEIGHT - lcd_scroll_bottom) {
        uint16_t y_virtual = (lcd_y_offset + y) % lcd_memory_scroll_height;
        uint16_t y_end = lcd_scroll_top + y_virtual + height - 1;
        if (y_end >= lcd_scroll_top + lcd_memory_scroll_height) {
            y_end = lcd_scroll_top + lcd_memory_scroll_height - 1;
        }
        lcd_set_window(x, lcd_scroll_top + y_virtual, x + width - 1, y_end);
    } else {
        lcd_set_window(x, y, x + width - 1, y + height - 1);
    }

    lcd_write16_buf((uint16_t *)pixels, width * height);
    lcd_enable_interrupts();
}

void lcd_solid_rectangle(uint16_t colour, uint16_t x, uint16_t y, uint16_t width, uint16_t height) {
    static uint16_t pixels[WIDTH];

    for (uint16_t row = 0; row < height; row++) {
        for (uint16_t i = 0; i < width; i++) {
            pixels[i] = colour;
        }
        lcd_blit(pixels, x, y + row, width, 1);
    }
}

//
// Scrolling functions
//

void lcd_define_scrolling(uint16_t top_fixed_area, uint16_t bottom_fixed_area) {
    uint16_t scroll_area = HEIGHT - (top_fixed_area + bottom_fixed_area);
    if (scroll_area == 0 || scroll_area > FRAME_HEIGHT) {
        top_fixed_area = 0;
        bottom_fixed_area = 0;
        scroll_area = FRAME_HEIGHT;
    }
    
    lcd_scroll_top = top_fixed_area;
    lcd_memory_scroll_height = FRAME_HEIGHT - (top_fixed_area + bottom_fixed_area);
    lcd_scroll_bottom = bottom_fixed_area;

    lcd_disable_interrupts();
    lcd_write_cmd(LCD_CMD_VSCRDEF);
    lcd_write_data(6,
                   UPPER8(lcd_scroll_top),
                   LOWER8(lcd_scroll_top),
                   UPPER8(scroll_area),
                   LOWER8(scroll_area),
                   UPPER8(lcd_scroll_bottom),
                   LOWER8(lcd_scroll_bottom));
    lcd_enable_interrupts();

    lcd_scroll_reset();
}

void lcd_scroll_reset() {
    lcd_y_offset = 0;
    uint16_t scroll_area_start = lcd_scroll_top + lcd_y_offset;

    lcd_disable_interrupts();
    lcd_write_cmd(LCD_CMD_VSCSAD);
    lcd_write_data(2, UPPER8(scroll_area_start), LOWER8(scroll_area_start));
    lcd_enable_interrupts();
}

void lcd_scroll_clear() {
    lcd_scroll_reset();
    lcd_solid_rectangle(background, 0, lcd_scroll_top, WIDTH, lcd_memory_scroll_height);
}

void lcd_scroll_up(void) {
    if (lcd_memory_scroll_height == 0) {
        return;
    }
    lcd_y_offset = (lcd_y_offset + GLYPH_HEIGHT) % lcd_memory_scroll_height;
    uint16_t scroll_area_start = lcd_scroll_top + lcd_y_offset;

    lcd_disable_interrupts();
    lcd_write_cmd(LCD_CMD_VSCSAD);
    lcd_write_data(2, UPPER8(scroll_area_start), LOWER8(scroll_area_start));
    lcd_enable_interrupts();

    lcd_solid_rectangle(background, 0, HEIGHT - GLYPH_HEIGHT, WIDTH, GLYPH_HEIGHT);
}

void lcd_scroll_down(void) {
    if (lcd_memory_scroll_height == 0) {
        return;
    }
    lcd_y_offset = (lcd_y_offset - GLYPH_HEIGHT + lcd_memory_scroll_height) % lcd_memory_scroll_height;
    uint16_t scroll_area_start = lcd_scroll_top + lcd_y_offset;

    lcd_disable_interrupts();
    lcd_write_cmd(LCD_CMD_VSCSAD);
    lcd_write_data(2, UPPER8(scroll_area_start), LOWER8(scroll_area_start));
    lcd_enable_interrupts();

    lcd_solid_rectangle(background, 0, lcd_scroll_top, WIDTH, GLYPH_HEIGHT);
}

//
// Text drawing functions
//

void lcd_clear_screen() {
    lcd_scroll_reset();
    lcd_solid_rectangle(background, 0, 0, WIDTH, FRAME_HEIGHT);
}

void lcd_erase_line(uint8_t row, uint8_t col_start, uint8_t col_end) {
    lcd_solid_rectangle(background, col_start * font->width, row * GLYPH_HEIGHT, 
                       (col_end - col_start + 1) * font->width, GLYPH_HEIGHT);
}

void lcd_putc(uint8_t column, uint8_t row, uint8_t c) {
    const uint8_t *glyph = &font->glyphs[c * GLYPH_HEIGHT];
    uint16_t *buffer = char_buffer;

    if (font->width == 8) {
        for (uint8_t i = 0; i < GLYPH_HEIGHT; i++, glyph++) {
            if (i < GLYPH_HEIGHT - 1) {
                *(buffer++) = (*glyph & 0x80) ? foreground : background;
                *(buffer++) = (*glyph & 0x40) || (bold && (*glyph & 0x80)) ? foreground : background;
                *(buffer++) = (*glyph & 0x20) || (bold && (*glyph & 0x40)) ? foreground : background;
                *(buffer++) = (*glyph & 0x10) || (bold && (*glyph & 0x20)) ? foreground : background;
                *(buffer++) = (*glyph & 0x08) || (bold && (*glyph & 0x10)) ? foreground : background;
                *(buffer++) = (*glyph & 0x04) || (bold && (*glyph & 0x08)) ? foreground : background;
                *(buffer++) = (*glyph & 0x02) || (bold && (*glyph & 0x04)) ? foreground : background;
                *(buffer++) = (*glyph & 0x01) || (bold && (*glyph & 0x02)) ? foreground : background;
            } else {
                *(buffer++) = (*glyph & 0x80) || underscore ? foreground : background;
                *(buffer++) = (*glyph & 0x40) || underscore ? foreground : background;
                *(buffer++) = (*glyph & 0x20) || underscore ? foreground : background;
                *(buffer++) = (*glyph & 0x10) || underscore ? foreground : background;
                *(buffer++) = (*glyph & 0x08) || underscore ? foreground : background;
                *(buffer++) = (*glyph & 0x04) || underscore ? foreground : background;
                *(buffer++) = (*glyph & 0x02) || underscore ? foreground : background;
                *(buffer++) = (*glyph & 0x01) || underscore ? foreground : background;
            }
        }
    } else {
        for (uint8_t i = 0; i < GLYPH_HEIGHT; i++, glyph++) {
            if (i < GLYPH_HEIGHT - 1) {
                *(buffer++) = (*glyph & 0x10) ? foreground : background;
                *(buffer++) = (*glyph & 0x08) ? foreground : background;
                *(buffer++) = (*glyph & 0x04) ? foreground : background;
                *(buffer++) = (*glyph & 0x02) ? foreground : background;
                *(buffer++) = (*glyph & 0x01) ? foreground : background;
            } else {
                *(buffer++) = (*glyph & 0x10) || underscore ? foreground : background;
                *(buffer++) = (*glyph & 0x08) || underscore ? foreground : background;
                *(buffer++) = (*glyph & 0x04) || underscore ? foreground : background;
                *(buffer++) = (*glyph & 0x02) || underscore ? foreground : background;
                *(buffer++) = (*glyph & 0x01) || underscore ? foreground : background;
            }
        }
    }

    lcd_blit(char_buffer, column * font->width, row * GLYPH_HEIGHT, font->width, GLYPH_HEIGHT);
}

void lcd_putstr(uint8_t column, uint8_t row, const char *str) {
    int len = strlen(str);
    int pos = 0;
    while (*str) {
        uint16_t *buffer = line_buffer + (pos++ * font->width);
        const uint8_t *glyph = &font->glyphs[*str++ * GLYPH_HEIGHT];

        if (font->width == 8) {
            for (uint8_t i = 0; i < GLYPH_HEIGHT; i++, glyph++) {
                if (i < GLYPH_HEIGHT - 1) {
                    *(buffer++) = (*glyph & 0x80) ? foreground : background;
                    *(buffer++) = (*glyph & 0x40) || (bold && (*glyph & 0x80)) ? foreground : background;
                    *(buffer++) = (*glyph & 0x20) || (bold && (*glyph & 0x40)) ? foreground : background;
                    *(buffer++) = (*glyph & 0x10) || (bold && (*glyph & 0x20)) ? foreground : background;
                    *(buffer++) = (*glyph & 0x08) || (bold && (*glyph & 0x10)) ? foreground : background;
                    *(buffer++) = (*glyph & 0x04) || (bold && (*glyph & 0x08)) ? foreground : background;
                    *(buffer++) = (*glyph & 0x02) || (bold && (*glyph & 0x04)) ? foreground : background;
                    *(buffer++) = (*glyph & 0x01) || (bold && (*glyph & 0x02)) ? foreground : background;
                } else {
                    *(buffer++) = (*glyph & 0x80) || underscore ? foreground : background;
                    *(buffer++) = (*glyph & 0x40) || underscore ? foreground : background;
                    *(buffer++) = (*glyph & 0x20) || underscore ? foreground : background;
                    *(buffer++) = (*glyph & 0x10) || underscore ? foreground : background;
                    *(buffer++) = (*glyph & 0x08) || underscore ? foreground : background;
                    *(buffer++) = (*glyph & 0x04) || underscore ? foreground : background;
                    *(buffer++) = (*glyph & 0x02) || underscore ? foreground : background;
                    *(buffer++) = (*glyph & 0x01) || underscore ? foreground : background;
                }
                buffer += (len - 1) * font->width;
            }
        } else {
            for (uint8_t i = 0; i < GLYPH_HEIGHT; i++, glyph++) {
                if (i < GLYPH_HEIGHT - 1) {
                    *(buffer++) = (*glyph & 0x10) ? foreground : background;
                    *(buffer++) = (*glyph & 0x08) ? foreground : background;
                    *(buffer++) = (*glyph & 0x04) ? foreground : background;
                    *(buffer++) = (*glyph & 0x02) ? foreground : background;
                    *(buffer++) = (*glyph & 0x01) ? foreground : background;
                } else {
                    *(buffer++) = (*glyph & 0x10) || underscore ? foreground : background;
                    *(buffer++) = (*glyph & 0x08) || underscore ? foreground : background;
                    *(buffer++) = (*glyph & 0x04) || underscore ? foreground : background;
                    *(buffer++) = (*glyph & 0x02) || underscore ? foreground : background;
                    *(buffer++) = (*glyph & 0x01) || underscore ? foreground : background;
                }
                buffer += (len - 1) * font->width;
            }
        }
    }

    if (len) {
        lcd_blit(line_buffer, column * font->width, row * GLYPH_HEIGHT, font->width * len, GLYPH_HEIGHT);
    }
}

//
// Cursor functions
//

void lcd_enable_cursor(bool cursor_on) {
    cursor_enabled = cursor_on;
}

bool lcd_cursor_enabled() {
    return cursor_enabled;
}

void lcd_move_cursor(uint8_t column, uint8_t row) {
    uint8_t max_col = lcd_get_columns() - 1;
    cursor_column = column;
    cursor_row = row;

    if (cursor_column > max_col)
        cursor_column = max_col;
    if (cursor_row > MAX_ROW)
        cursor_row = MAX_ROW;
}

void lcd_draw_cursor() {
    if (cursor_enabled) {
        lcd_solid_rectangle(foreground, cursor_column * font->width, 
                          ((cursor_row + 1) * GLYPH_HEIGHT) - 1, font->width, 1);
    }
}

void lcd_erase_cursor() {
    if (cursor_enabled) {
        lcd_solid_rectangle(background, cursor_column * font->width, 
                          ((cursor_row + 1) * GLYPH_HEIGHT) - 1, font->width, 1);
    }
}

//
// Display control functions
//

void lcd_reset() {
    gpio_set_value(LCD_RST, 0);
    sleep_us(20);

    gpio_set_value(LCD_RST, 1);
    sleep_ms(120);
}

void lcd_display_on() {
    lcd_disable_interrupts();
    lcd_write_cmd(LCD_CMD_DISPON);
    lcd_enable_interrupts();
}

void lcd_display_off() {
    lcd_disable_interrupts();
    lcd_write_cmd(LCD_CMD_DISPOFF);
    lcd_enable_interrupts();
}

//
// Cursor timer callback - TODO: Implement with RK3506 timer
//
void lcd_cursor_timer_callback(void) {
    if (!lcd_cursor_enabled()) {
        return;
    }

    if (cursor_visible) {
        lcd_erase_cursor();
    } else {
        lcd_draw_cursor();
    }

    cursor_visible = !cursor_visible;
}

//
// Initialization
//

// TODO: Replace with your RK3506 GPIO/SPI initialization
static void gpio_init_output(uint32_t pin) {
    // Configure pin as output
}

static void spi_init_hw(void) {
    // Initialize SPI1 peripheral
    // Configure baud rate, mode, etc.
}

void lcd_init() {
    if (lcd_initialised) {
        return;
    }

    // Initialize GPIO pins
    gpio_init_output(LCD_SCL);
    gpio_init_output(LCD_SDI);
    gpio_init_output(LCD_CSX);
    gpio_init_output(LCD_DCX);
    gpio_init_output(LCD_RST);

    // Initialize SPI
    spi_init_hw();
    
    // TODO: Configure pins for SPI function (mux)
    // set_pin_function(LCD_SCL, PIN_FUNC_SPI);
    // set_pin_function(LCD_SDI, PIN_FUNC_SPI);
    // set_pin_function(LCD_SDO, PIN_FUNC_SPI);

    gpio_set_value(LCD_CSX, 1);
    gpio_set_value(LCD_RST, 1);

    lcd_disable_interrupts();

    lcd_reset();

    lcd_write_cmd(LCD_CMD_SWRESET);
    sleep_ms(10);

    lcd_write_cmd(LCD_CMD_COLMOD);
    lcd_write_data(1, 0x55);

    lcd_write_cmd(LCD_CMD_MADCTL);
    lcd_write_data(1, 0x48);

    lcd_write_cmd(LCD_CMD_INVON);

    lcd_write_cmd(LCD_CMD_EMS);
    lcd_write_data(1, 0xC6);

    lcd_write_cmd(LCD_CMD_VSCRDEF);
    lcd_write_data(6,
                   0x00, 0x00,
                   0x01, 0x40,
                   0x00, 0x00);

    lcd_write_cmd(LCD_CMD_SLPOUT);
    lcd_enable_interrupts();

    sleep_ms(10);

    lcd_clear_screen();
    lcd_display_on();

    // TODO: Setup timer for cursor blinking (500ms interval)
    // register_timer_callback(500, lcd_cursor_timer_callback);

    lcd_initialised = true;
}