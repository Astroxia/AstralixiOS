#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>

// Display dimensions (PicoCalc specific)
#define DISPLAY_WIDTH_PIXELS  320
#define DISPLAY_HEIGHT_PIXELS 240
#define DISPLAY_WIDTH_CHARS   40   // 320/8 = 40 chars at 8 pixels wide
#define DISPLAY_HEIGHT_CHARS  24   // 240/10 = 24 chars at 10 pixels tall

// ANSI color codes
#define COLOR_BLACK     0
#define COLOR_RED       1
#define COLOR_GREEN     2
#define COLOR_YELLOW    3
#define COLOR_BLUE      4
#define COLOR_MAGENTA   5
#define COLOR_CYAN      6
#define COLOR_WHITE     7
#define COLOR_DEFAULT   9

// Display attributes
#define ATTR_NORMAL     0x00
#define ATTR_BOLD       0x01
#define ATTR_DIM        0x02
#define ATTR_ITALIC     0x04
#define ATTR_UNDERLINE  0x08
#define ATTR_BLINK      0x10
#define ATTR_REVERSE    0x20
#define ATTR_HIDDEN     0x40
#define ATTR_STRIKE     0x80

// Character cell structure
typedef struct {
    uint8_t character;      // ASCII character
    uint8_t fg_color;       // Foreground color (0-15)
    uint8_t bg_color;       // Background color (0-15)
    uint8_t attributes;     // Text attributes
} char_cell_t;

// Display modes
typedef enum {
    DISPLAY_MODE_TEXT,      // Text mode with ANSI support
    DISPLAY_MODE_GRAPHICS,  // Raw pixel mode
    DISPLAY_MODE_MIXED      // Mixed text/graphics
} display_mode_t;

// Cursor styles
typedef enum {
    CURSOR_NONE,
    CURSOR_UNDERLINE,
    CURSOR_BLOCK,
    CURSOR_BLINK_UNDERLINE,
    CURSOR_BLINK_BLOCK
} cursor_style_t;

// Display configuration
typedef struct {
    display_mode_t mode;
    bool auto_scroll;
    bool wrap_lines;
    cursor_style_t cursor_style;
    uint8_t tab_width;
    bool ansi_enabled;
    uint8_t default_fg;
    uint8_t default_bg;
} display_config_t;

// Display context structure
typedef struct {
    // Hardware interface
    void *lcd_ctx;              // LCD driver context
    
    // Text buffer
    char_cell_t *text_buffer;   // Character buffer
    uint16_t buffer_size;
    
    // Cursor position
    uint16_t cursor_x;          // Column (0-39)
    uint16_t cursor_y;          // Row (0-23)
    bool cursor_visible;
    cursor_style_t cursor_style;
    
    // Current attributes
    uint8_t current_fg;
    uint8_t current_bg;
    uint8_t current_attr;
    
    // ANSI escape sequence parsing
    bool in_escape;
    char escape_buffer[32];
    int escape_index;
    
    // Configuration
    display_config_t config;
    
    // Thread safety
    pthread_mutex_t lock;
    
    // Dirty tracking for optimization
    bool *dirty_lines;
    bool full_redraw_needed;
    
    // Scroll region
    uint16_t scroll_top;
    uint16_t scroll_bottom;
} display_context_t;

// Function prototypes

// Initialization
int display_init(display_context_t *ctx, void *lcd_ctx);
void display_deinit(display_context_t *ctx);

// Basic operations
void display_clear(display_context_t *ctx);
void display_clear_line(display_context_t *ctx, uint16_t line);
void display_scroll_up(display_context_t *ctx, uint16_t lines);
void display_scroll_down(display_context_t *ctx, uint16_t lines);

// Text output
void display_putchar(display_context_t *ctx, char c);
void display_puts(display_context_t *ctx, const char *str);
void display_printf(display_context_t *ctx, const char *format, ...);
void display_write(display_context_t *ctx, const char *data, size_t len);

// Cursor control
void display_set_cursor(display_context_t *ctx, uint16_t x, uint16_t y);
void display_get_cursor(display_context_t *ctx, uint16_t *x, uint16_t *y);
void display_show_cursor(display_context_t *ctx, bool show);
void display_set_cursor_style(display_context_t *ctx, cursor_style_t style);

// Attributes and colors
void display_set_fg_color(display_context_t *ctx, uint8_t color);
void display_set_bg_color(display_context_t *ctx, uint8_t color);
void display_set_colors(display_context_t *ctx, uint8_t fg, uint8_t bg);
void display_set_attribute(display_context_t *ctx, uint8_t attr);
void display_reset_attributes(display_context_t *ctx);

// ANSI escape sequence support
void display_process_ansi(display_context_t *ctx, const char *seq);
void display_enable_ansi(display_context_t *ctx, bool enable);

// Direct buffer access
char_cell_t* display_get_cell(display_context_t *ctx, uint16_t x, uint16_t y);
void display_set_cell(display_context_t *ctx, uint16_t x, uint16_t y, 
                     char c, uint8_t fg, uint8_t bg, uint8_t attr);

// Window and region management
void display_set_scroll_region(display_context_t *ctx, uint16_t top, uint16_t bottom);
void display_reset_scroll_region(display_context_t *ctx);

// Configuration
void display_set_config(display_context_t *ctx, const display_config_t *config);
void display_get_config(display_context_t *ctx, display_config_t *config);

// Rendering
void display_update(display_context_t *ctx);
void display_update_line(display_context_t *ctx, uint16_t line);
void display_force_redraw(display_context_t *ctx);

// Special characters and box drawing
void display_draw_box(display_context_t *ctx, uint16_t x, uint16_t y, 
                     uint16_t width, uint16_t height);
void display_draw_hline(display_context_t *ctx, uint16_t x, uint16_t y, uint16_t len);
void display_draw_vline(display_context_t *ctx, uint16_t x, uint16_t y, uint16_t len);

// Utility functions
uint32_t display_rgb_to_color(uint8_t r, uint8_t g, uint8_t b);
void display_save_state(display_context_t *ctx);
void display_restore_state(display_context_t *ctx);

#endif // DISPLAY_H