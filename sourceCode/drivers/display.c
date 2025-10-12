#include "display.h"
#include "lcd.h"
#include "font-8x10.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

// ANSI color palette (16 colors)
static const uint32_t ansi_palette[16] = {
    0x000000, // 0: Black
    0xAA0000, // 1: Red
    0x00AA00, // 2: Green
    0xAA5500, // 3: Yellow/Brown
    0x0000AA, // 4: Blue
    0xAA00AA, // 5: Magenta
    0x00AAAA, // 6: Cyan
    0xAAAAAA, // 7: White (light gray)
    0x555555, // 8: Bright black (dark gray)
    0xFF5555, // 9: Bright red
    0x55FF55, // 10: Bright green
    0xFFFF55, // 11: Bright yellow
    0x5555FF, // 12: Bright blue
    0xFF55FF, // 13: Bright magenta
    0x55FFFF, // 14: Bright cyan
    0xFFFFFF  // 15: Bright white
};

// Box drawing characters
#define BOX_HORIZONTAL      0xC4
#define BOX_VERTICAL        0xB3
#define BOX_TOP_LEFT        0xDA
#define BOX_TOP_RIGHT       0xBF
#define BOX_BOTTOM_LEFT     0xC0
#define BOX_BOTTOM_RIGHT    0xD9
#define BOX_CROSS           0xC5
#define BOX_T_LEFT          0xC3
#define BOX_T_RIGHT         0xB4
#define BOX_T_TOP           0xC2
#define BOX_T_BOTTOM        0xC1

// Saved cursor state
typedef struct {
    uint16_t x;
    uint16_t y;
    uint8_t fg;
    uint8_t bg;
    uint8_t attr;
} cursor_state_t;

static cursor_state_t saved_state;

int display_init(display_context_t *ctx, void *lcd_ctx) {
    if (!ctx || !lcd_ctx) {
        return -1;
    }
    
    memset(ctx, 0, sizeof(display_context_t));
    ctx->lcd_ctx = lcd_ctx;
    
    // Allocate text buffer
    ctx->buffer_size = DISPLAY_WIDTH_CHARS * DISPLAY_HEIGHT_CHARS;
    ctx->text_buffer = calloc(ctx->buffer_size, sizeof(char_cell_t));
    if (!ctx->text_buffer) {
        return -1;
    }
    
    // Allocate dirty line tracking
    ctx->dirty_lines = calloc(DISPLAY_HEIGHT_CHARS, sizeof(bool));
    if (!ctx->dirty_lines) {
        free(ctx->text_buffer);
        return -1;
    }
    
    // Initialize mutex
    if (pthread_mutex_init(&ctx->lock, NULL) != 0) {
        free(ctx->text_buffer);
        free(ctx->dirty_lines);
        return -1;
    }
    
    // Set default configuration
    ctx->config.mode = DISPLAY_MODE_TEXT;
    ctx->config.auto_scroll = true;
    ctx->config.wrap_lines = true;
    ctx->config.cursor_style = CURSOR_BLINK_BLOCK;
    ctx->config.tab_width = 8;
    ctx->config.ansi_enabled = true;
    ctx->config.default_fg = COLOR_WHITE;
    ctx->config.default_bg = COLOR_BLACK;
    
    // Set initial attributes
    ctx->current_fg = ctx->config.default_fg;
    ctx->current_bg = ctx->config.default_bg;
    ctx->current_attr = ATTR_NORMAL;
    
    // Set scroll region to full screen
    ctx->scroll_top = 0;
    ctx->scroll_bottom = DISPLAY_HEIGHT_CHARS - 1;
    
    // Clear display
    display_clear(ctx);
    
    return 0;
}

void display_deinit(display_context_t *ctx) {
    if (!ctx) {
        return;
    }
    
    pthread_mutex_destroy(&ctx->lock);
    free(ctx->text_buffer);
    free(ctx->dirty_lines);
}

void display_clear(display_context_t *ctx) {
    if (!ctx) {
        return;
    }
    
    pthread_mutex_lock(&ctx->lock);
    
    // Clear text buffer
    for (int i = 0; i < ctx->buffer_size; i++) {
        ctx->text_buffer[i].character = ' ';
        ctx->text_buffer[i].fg_color = ctx->config.default_fg;
        ctx->text_buffer[i].bg_color = ctx->config.default_bg;
        ctx->text_buffer[i].attributes = ATTR_NORMAL;
    }
    
    // Reset cursor
    ctx->cursor_x = 0;
    ctx->cursor_y = 0;
    
    // Mark all lines as dirty
    for (int i = 0; i < DISPLAY_HEIGHT_CHARS; i++) {
        ctx->dirty_lines[i] = true;
    }
    ctx->full_redraw_needed = true;
    
    pthread_mutex_unlock(&ctx->lock);
    
    // Update display
    display_update(ctx);
}

void display_clear_line(display_context_t *ctx, uint16_t line) {
    if (!ctx || line >= DISPLAY_HEIGHT_CHARS) {
        return;
    }
    
    pthread_mutex_lock(&ctx->lock);
    
    int offset = line * DISPLAY_WIDTH_CHARS;
    for (int x = 0; x < DISPLAY_WIDTH_CHARS; x++) {
        ctx->text_buffer[offset + x].character = ' ';
        ctx->text_buffer[offset + x].fg_color = ctx->config.default_fg;
        ctx->text_buffer[offset + x].bg_color = ctx->config.default_bg;
        ctx->text_buffer[offset + x].attributes = ATTR_NORMAL;
    }
    
    ctx->dirty_lines[line] = true;
    
    pthread_mutex_unlock(&ctx->lock);
}

void display_scroll_up(display_context_t *ctx, uint16_t lines) {
    if (!ctx || lines == 0) {
        return;
    }
    
    pthread_mutex_lock(&ctx->lock);
    
    uint16_t scroll_height = ctx->scroll_bottom - ctx->scroll_top + 1;
    if (lines > scroll_height) {
        lines = scroll_height;
    }
    
    // Move lines up within scroll region
    for (uint16_t y = ctx->scroll_top; y <= ctx->scroll_bottom - lines; y++) {
        memcpy(&ctx->text_buffer[y * DISPLAY_WIDTH_CHARS],
               &ctx->text_buffer[(y + lines) * DISPLAY_WIDTH_CHARS],
               DISPLAY_WIDTH_CHARS * sizeof(char_cell_t));
        ctx->dirty_lines[y] = true;
    }
    
    // Clear bottom lines
    for (uint16_t y = ctx->scroll_bottom - lines + 1; y <= ctx->scroll_bottom; y++) {
        int offset = y * DISPLAY_WIDTH_CHARS;
        for (int x = 0; x < DISPLAY_WIDTH_CHARS; x++) {
            ctx->text_buffer[offset + x].character = ' ';
            ctx->text_buffer[offset + x].fg_color = ctx->config.default_fg;
            ctx->text_buffer[offset + x].bg_color = ctx->config.default_bg;
            ctx->text_buffer[offset + x].attributes = ATTR_NORMAL;
        }
        ctx->dirty_lines[y] = true;
    }
    
    pthread_mutex_unlock(&ctx->lock);
}

void display_putchar(display_context_t *ctx, char c) {
    if (!ctx) {
        return;
    }
    
    pthread_mutex_lock(&ctx->lock);
    
    // Handle ANSI escape sequences
    if (ctx->config.ansi_enabled) {
        if (ctx->in_escape) {
            if (ctx->escape_index < 31) {
                ctx->escape_buffer[ctx->escape_index++] = c;
                ctx->escape_buffer[ctx->escape_index] = '\0';
                
                // Check if sequence is complete
                if (isalpha(c) || c == 'm' || c == 'H' || c == 'J' || c == 'K') {
                    display_process_ansi(ctx, ctx->escape_buffer);
                    ctx->in_escape = false;
                    ctx->escape_index = 0;
                }
            } else {
                // Escape sequence too long, abort
                ctx->in_escape = false;
                ctx->escape_index = 0;
            }
            pthread_mutex_unlock(&ctx->lock);
            return;
        }
        
        if (c == '\033') {
            ctx->in_escape = true;
            ctx->escape_index = 0;
            pthread_mutex_unlock(&ctx->lock);
            return;
        }
    }
    
    // Handle special characters
    switch (c) {
        case '\n':
            ctx->cursor_x = 0;
            ctx->cursor_y++;
            break;
            
        case '\r':
            ctx->cursor_x = 0;
            break;
            
        case '\t':
            ctx->cursor_x = ((ctx->cursor_x / ctx->config.tab_width) + 1) * ctx->config.tab_width;
            if (ctx->cursor_x >= DISPLAY_WIDTH_CHARS) {
                ctx->cursor_x = 0;
                ctx->cursor_y++;
            }
            break;
            
        case '\b':
            if (ctx->cursor_x > 0) {
                ctx->cursor_x--;
            }
            break;
            
        case '\f':
            // Form feed - clear screen
            pthread_mutex_unlock(&ctx->lock);
            display_clear(ctx);
            return;
            
        default:
            // Normal character
            if (c >= 32) {  // Printable character
                int offset = ctx->cursor_y * DISPLAY_WIDTH_CHARS + ctx->cursor_x;
                ctx->text_buffer[offset].character = c;
                ctx->text_buffer[offset].fg_color = ctx->current_fg;
                ctx->text_buffer[offset].bg_color = ctx->current_bg;
                ctx->text_buffer[offset].attributes = ctx->current_attr;
                ctx->dirty_lines[ctx->cursor_y] = true;
                
                ctx->cursor_x++;
            }
            break;
    }
    
    // Handle line wrap
    if (ctx->cursor_x >= DISPLAY_WIDTH_CHARS) {
        if (ctx->config.wrap_lines) {
            ctx->cursor_x = 0;
            ctx->cursor_y++;
        } else {
            ctx->cursor_x = DISPLAY_WIDTH_CHARS - 1;
        }
    }
    
    // Handle scrolling
    if (ctx->cursor_y > ctx->scroll_bottom) {
        display_scroll_up(ctx, ctx->cursor_y - ctx->scroll_bottom);
        ctx->cursor_y = ctx->scroll_bottom;
    }
    
    pthread_mutex_unlock(&ctx->lock);
}

void display_puts(display_context_t *ctx, const char *str) {
    if (!ctx || !str) {
        return;
    }
    
    while (*str) {
        display_putchar(ctx, *str++);
    }
}

void display_printf(display_context_t *ctx, const char *format, ...) {
    if (!ctx || !format) {
        return;
    }
    
    char buffer[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    display_puts(ctx, buffer);
}

void display_process_ansi(display_context_t *ctx, const char *seq) {
    if (!seq || seq[0] != '[') {
        return;
    }
    
    // Parse ANSI escape sequence
    int params[16] = {0};
    int param_count = 0;
    const char *p = seq + 1;
    
    // Extract parameters
    while (*p && param_count < 16) {
        if (isdigit(*p)) {
            params[param_count] = params[param_count] * 10 + (*p - '0');
        } else if (*p == ';') {
            param_count++;
        } else {
            break;
        }
        p++;
    }
    if (param_count < 16 && params[param_count] > 0) {
        param_count++;
    }
    
    // Process command
    switch (*p) {
        case 'm': // Set graphics mode
            for (int i = 0; i < param_count; i++) {
                switch (params[i]) {
                    case 0: // Reset
                        ctx->current_attr = ATTR_NORMAL;
                        ctx->current_fg = ctx->config.default_fg;
                        ctx->current_bg = ctx->config.default_bg;
                        break;
                    case 1: ctx->current_attr |= ATTR_BOLD; break;
                    case 2: ctx->current_attr |= ATTR_DIM; break;
                    case 3: ctx->current_attr |= ATTR_ITALIC; break;
                    case 4: ctx->current_attr |= ATTR_UNDERLINE; break;
                    case 5: ctx->current_attr |= ATTR_BLINK; break;
                    case 7: ctx->current_attr |= ATTR_REVERSE; break;
                    case 8: ctx->current_attr |= ATTR_HIDDEN; break;
                    case 9: ctx->current_attr |= ATTR_STRIKE; break;
                    case 30: case 31: case 32: case 33:
                    case 34: case 35: case 36: case 37:
                        ctx->current_fg = params[i] - 30;
                        break;
                    case 39: ctx->current_fg = ctx->config.default_fg; break;
                    case 40: case 41: case 42: case 43:
                    case 44: case 45: case 46: case 47:
                        ctx->current_bg = params[i] - 40;
                        break;
                    case 49: ctx->current_bg = ctx->config.default_bg; break;
                    case 90: case 91: case 92: case 93:
                    case 94: case 95: case 96: case 97:
                        ctx->current_fg = params[i] - 90 + 8;
                        break;
                    case 100: case 101: case 102: case 103:
                    case 104: case 105: case 106: case 107:
                        ctx->current_bg = params[i] - 100 + 8;
                        break;
                }
            }
            break;
            
        case 'H': case 'f': // Set cursor position
            if (param_count >= 2) {
                ctx->cursor_y = (params[0] > 0) ? params[0] - 1 : 0;
                ctx->cursor_x = (params[1] > 0) ? params[1] - 1 : 0;
            } else {
                ctx->cursor_x = 0;
                ctx->cursor_y = 0;
            }
            break;
            
        case 'A': // Cursor up
            ctx->cursor_y -= (params[0] > 0) ? params[0] : 1;
            if (ctx->cursor_y < 0) ctx->cursor_y = 0;
            break;
            
        case 'B': // Cursor down
            ctx->cursor_y += (params[0] > 0) ? params[0] : 1;
            if (ctx->cursor_y >= DISPLAY_HEIGHT_CHARS) 
                ctx->cursor_y = DISPLAY_HEIGHT_CHARS - 1;
            break;
            
        case 'C': // Cursor right
            ctx->cursor_x += (params[0] > 0) ? params[0] : 1;
            if (ctx->cursor_x >= DISPLAY_WIDTH_CHARS) 
                ctx->cursor_x = DISPLAY_WIDTH_CHARS - 1;
            break;
            
        case 'D': // Cursor left
            ctx->cursor_x -= (params[0] > 0) ? params[0] : 1;
            if (ctx->cursor_x < 0) ctx->cursor_x = 0;
            break;
            
        case 'J': // Clear screen
            switch (params[0]) {
                case 0: // Clear from cursor to end
                    for (int y = ctx->cursor_y; y < DISPLAY_HEIGHT_CHARS; y++) {
                        int start = (y == ctx->cursor_y) ? ctx->cursor_x : 0;
                        for (int x = start; x < DISPLAY_WIDTH_CHARS; x++) {
                            int offset = y * DISPLAY_WIDTH_CHARS + x;
                            ctx->text_buffer[offset].character = ' ';
                            ctx->text_buffer[offset].fg_color = ctx->config.default_fg;
                            ctx->text_buffer[offset].bg_color = ctx->config.default_bg;
                            ctx->text_buffer[offset].attributes = ATTR_NORMAL;
                        }
                        ctx->dirty_lines[y] = true;
                    }
                    break;
                case 1: // Clear from start to cursor
                    for (int y = 0; y <= ctx->cursor_y; y++) {
                        int end = (y == ctx->cursor_y) ? ctx->cursor_x : DISPLAY_WIDTH_CHARS;
                        for (int x = 0; x < end; x++) {
                            int offset = y * DISPLAY_WIDTH_CHARS + x;
                            ctx->text_buffer[offset].character = ' ';
                            ctx->text_buffer[offset].fg_color = ctx->config.default_fg;
                            ctx->text_buffer[offset].bg_color = ctx->config.default_bg;
                            ctx->text_buffer[offset].attributes = ATTR_NORMAL;
                        }
                        ctx->dirty_lines[y] = true;
                    }
                    break;
                case 2: // Clear entire screen
                    for (int i = 0; i < ctx->buffer_size; i++) {
                        ctx->text_buffer[i].character = ' ';
                        ctx->text_buffer[i].fg_color = ctx->config.default_fg;
                        ctx->text_buffer[i].bg_color = ctx->config.default_bg;
                        ctx->text_buffer[i].attributes = ATTR_NORMAL;
                    }
                    ctx->full_redraw_needed = true;
                    break;
            }
            break;
            
        case 'K': // Clear line
            switch (params[0]) {
                case 0: // Clear from cursor to end of line
                    for (int x = ctx->cursor_x; x < DISPLAY_WIDTH_CHARS; x++) {
                        int offset = ctx->cursor_y * DISPLAY_WIDTH_CHARS + x;
                        ctx->text_buffer[offset].character = ' ';
                        ctx->text_buffer[offset].fg_color = ctx->config.default_fg;
                        ctx->text_buffer[offset].bg_color = ctx->config.default_bg;
                        ctx->text_buffer[offset].attributes = ATTR_NORMAL;
                    }
                    break;
                case 1: // Clear from start to cursor
                    for (int x = 0; x <= ctx->cursor_x; x++) {
                        int offset = ctx->cursor_y * DISPLAY_WIDTH_CHARS + x;
                        ctx->text_buffer[offset].character = ' ';
                        ctx->text_buffer[offset].fg_color = ctx->config.default_fg;
                        ctx->text_buffer[offset].bg_color = ctx->config.default_bg;
                        ctx->text_buffer[offset].attributes = ATTR_NORMAL;
                    }
                    break;
                case 2: // Clear entire line
                    display_clear_line(ctx, ctx->cursor_y);
                    break;
            }
            ctx->dirty_lines[ctx->cursor_y] = true;
            break;
    }
}

void display_update(display_context_t *ctx) {
    if (!ctx || !ctx->lcd_ctx) {
        return;
    }
    
    pthread_mutex_lock(&ctx->lock);
    
    lcd_context_t *lcd = (lcd_context_t*)ctx->lcd_ctx;
    
    // Update dirty lines
    for (int y = 0; y < DISPLAY_HEIGHT_CHARS; y++) {
        if (ctx->dirty_lines[y] || ctx->full_redraw_needed) {
            for (int x = 0; x < DISPLAY_WIDTH_CHARS; x++) {
                int offset = y * DISPLAY_WIDTH_CHARS + x;
                char_cell_t *cell = &ctx->text_buffer[offset];
                
                // Get colors
                uint32_t fg = ansi_palette[cell->fg_color & 0x0F];
                uint32_t bg = ansi_palette[cell->bg_color & 0x0F];
                
                // Handle reverse video
                if (cell->attributes & ATTR_REVERSE) {
                    uint32_t temp = fg;
                    fg = bg;
                    bg = temp;
                }
                
                // Handle bold (brighten foreground)
                if (cell->attributes & ATTR_BOLD) {
                    if ((cell->fg_color & 0x07) < 8) {
                        fg = ansi_palette[(cell->fg_color & 0x07) + 8];
                    }
                }
                
                // Draw character
                lcd_draw_char(lcd, x * 8, y * 10, cell->character, fg, bg);
            }
            ctx->dirty_lines[y] = false;
        }
    }
    
    // Draw cursor if visible
    if (ctx->cursor_visible && ctx->cursor_style != CURSOR_NONE) {
        int cx = ctx->cursor_x * 8;
        int cy = ctx->cursor_y * 10;
        uint32_t cursor_color = ansi_palette[COLOR_WHITE];
        
        switch (ctx->cursor_style) {
            case CURSOR_UNDERLINE:
            case CURSOR_BLINK_UNDERLINE:
                lcd_draw_hline(lcd, cx, cy + 9, 8, cursor_color);
                break;
            case CURSOR_BLOCK:
            case CURSOR_BLINK_BLOCK:
                lcd_draw_rect_filled(lcd, cx, cy, 8, 10, cursor_color);
                break;
            default:
                break;
        }
    }
    
    ctx->full_redraw_needed = false;
    
    pthread_mutex_unlock(&ctx->lock);
    
    // Update LCD
    lcd_update(lcd);
}

void display_set_cursor(display_context_t *ctx, uint16_t x, uint16_t y) {
    if (!ctx) {
        return;
    }
    
    pthread_mutex_lock(&ctx->lock);
    
    if (x < DISPLAY_WIDTH_CHARS) {
        ctx->cursor_x = x;
    }
    if (y < DISPLAY_HEIGHT_CHARS) {
        ctx->cursor_y = y;
    }
    
    pthread_mutex_unlock(&ctx->lock);
}

void display_get_cursor(display_context_t *ctx, uint16_t *x, uint16_t *y) {
    if (!ctx || !x || !y) {
        return;
    }
    
    pthread_mutex_lock(&ctx->lock);
    *x = ctx->cursor_x;
    *y = ctx->cursor_y;
    pthread_mutex_unlock(&ctx->lock);
}

void display_draw_box(display_context_t *ctx, uint16_t x, uint16_t y,
                     uint16_t width, uint16_t height) {
    if (!ctx || width < 2 || height < 2) {
        return;
    }
    
    pthread_mutex_lock(&ctx->lock);
    
    // Top line
    display_set_cell(ctx, x, y, BOX_TOP_LEFT, ctx->current_fg, ctx->current_bg, ctx->current_attr);
    for (uint16_t i = 1; i < width - 1; i++) {
        display_set_cell(ctx, x + i, y, BOX_HORIZONTAL, ctx->current_fg, ctx->current_bg, ctx->current_attr);
    }
    display_set_cell(ctx, x + width - 1, y, BOX_TOP_RIGHT, ctx->current_fg, ctx->current_bg, ctx->current_attr);
    
    // Vertical lines
    for (uint16_t i = 1; i < height - 1; i++) {
        display_set_cell(ctx, x, y + i, BOX_VERTICAL, ctx->current_fg, ctx->current_bg, ctx->current_attr);
        display_set_cell(ctx, x + width - 1, y + i, BOX_VERTICAL, ctx->current_fg, ctx->current_bg, ctx->current_attr);
    }
    
    // Bottom line
    display_set_cell(ctx, x, y + height - 1, BOX_BOTTOM_LEFT, ctx->current_fg, ctx->current_bg, ctx->current_attr);
    for (uint16_t i = 1; i < width - 1; i++) {
        display_set_cell(ctx, x + i, y + height - 1, BOX_HORIZONTAL, ctx->current_fg, ctx->current_bg, ctx->current_attr);
    }
    display_set_cell(ctx, x + width - 1, y + height - 1, BOX_BOTTOM_RIGHT, ctx->current_fg, ctx->current_bg, ctx->current_attr);
    
    // Mark affected lines as dirty
    for (uint16_t i = y; i < y + height && i < DISPLAY_HEIGHT_CHARS; i++) {
        ctx->dirty_lines[i] = true;
    }
    
    pthread_mutex_unlock(&ctx->lock);
}

void display_set_cell(display_context_t *ctx, uint16_t x, uint16_t y,
                     char c, uint8_t fg, uint8_t bg, uint8_t attr) {
    if (!ctx || x >= DISPLAY_WIDTH_CHARS || y >= DISPLAY_HEIGHT_CHARS) {
        return;
    }
    
    int offset = y * DISPLAY_WIDTH_CHARS + x;
    ctx->text_buffer[offset].character = c;
    ctx->text_buffer[offset].fg_color = fg;
    ctx->text_buffer[offset].bg_color = bg;
    ctx->text_buffer[offset].attributes = attr;
    ctx->dirty_lines[y] = true;
}

void display_save_state(display_context_t *ctx) {
    if (!ctx) {
        return;
    }
    
    pthread_mutex_lock(&ctx->lock);
    saved_state.x = ctx->cursor_x;
    saved_state.y = ctx->cursor_y;
    saved_state.fg = ctx->current_fg;
    saved_state.bg = ctx->current_bg;
    saved_state.attr = ctx->current_attr;
    pthread_mutex_unlock(&ctx->lock);
}

void display_restore_state(display_context_t *ctx) {
    if (!ctx) {
        return;
    }
    
    pthread_mutex_lock(&ctx->lock);
    ctx->cursor_x = saved_state.x;
    ctx->cursor_y = saved_state.y;
    ctx->current_fg = saved_state.fg;
    ctx->current_bg = saved_state.bg;
    ctx->current_attr = saved_state.attr;
    pthread_mutex_unlock(&ctx->lock);
}