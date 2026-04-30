#include "keyboard.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <time.h>
#include <errno.h>

// PicoCalc keyboard matrix mapping
// Based on the PicoCalc's 7x8 matrix layout
static const uint8_t keymap[KB_MATRIX_ROWS][KB_MATRIX_COLS] = {
    // Col 0    1      2      3      4      5      6      7
    { KEY_ESC,  '1',   '2',   '3',   '4',   '5',   '6',   KEY_BACKSPACE }, // Row 0
    { KEY_TAB,  'q',   'w',   'e',   'r',   't',   'y',   '7'           }, // Row 1  
    { KEY_CTRL, 'a',   's',   'd',   'f',   'g',   'h',   '8'           }, // Row 2
    { KEY_SHIFT,'z',   'x',   'c',   'v',   'b',   'n',   '9'           }, // Row 3
    { KEY_FN,   KEY_ALT, ' ', ',',   '.',   '/',   'm',   '0'           }, // Row 4
    { KEY_F1,   KEY_F2, KEY_F3, 'u', 'i',   'o',   'p',   KEY_DELETE    }, // Row 5
    { KEY_F4,   KEY_F5, KEY_F6, 'j', 'k',   'l',   ';',   KEY_RETURN    }  // Row 6
};

// Shift key mappings
static const uint8_t shift_keymap[KB_MATRIX_ROWS][KB_MATRIX_COLS] = {
    // Col 0    1      2      3      4      5      6      7
    { KEY_ESC,  '!',   '@',   '#',   '$',   '%',   '^',   KEY_BACKSPACE },
    { KEY_TAB,  'Q',   'W',   'E',   'R',   'T',   'Y',   '&'           },
    { KEY_CTRL, 'A',   'S',   'D',   'F',   'G',   'H',   '*'           },
    { KEY_SHIFT,'Z',   'X',   'C',   'V',   'B',   'N',   '('           },
    { KEY_FN,   KEY_ALT,' ',  '<',   '>',   '?',   'M',   ')'           },
    { KEY_F1,   KEY_F2, KEY_F3,'U',  'I',   'O',   'P',   KEY_DELETE    },
    { KEY_F4,   KEY_F5, KEY_F6,'J',  'K',   'L',   ':',   KEY_RETURN    }
};

// Function key mappings (when Fn is pressed)
static const uint8_t fn_keymap[KB_MATRIX_ROWS][KB_MATRIX_COLS] = {
    { KEY_BREAK, KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6, KEY_BACKSPACE },
    { KEY_TAB,   KEY_HOME, KEY_UP, KEY_END, KEY_PAGEUP, '`', '~', KEY_INSERT },
    { KEY_CTRL,  KEY_LEFT, KEY_DOWN, KEY_RIGHT, KEY_PAGEDOWN, '[', ']', '{' },
    { KEY_SHIFT, '\\', '|', '_', '-', '=', '+', '}' },
    { KEY_FN,    KEY_ALT, ' ', '<', '>', '/', '"', '\'' },
    { KEY_F1,    KEY_F2, KEY_F3, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_DELETE },
    { KEY_F4,    KEY_F5, KEY_F6, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_RETURN }
};

// Southbridge I2C commands for keyboard
#define SB_CMD_KB_SCAN      0x10  // Read keyboard matrix
#define SB_CMD_KB_LED       0x11  // Set keyboard LED
#define SB_CMD_KB_BACKLIGHT 0x12  // Set keyboard backlight

static uint32_t get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (ts.tv_sec * 1000) + (ts.tv_nsec / 1000000);
}

static int i2c_write_read(int fd, uint8_t addr, uint8_t *write_buf, size_t write_len, 
                         uint8_t *read_buf, size_t read_len) {
    if (ioctl(fd, I2C_SLAVE, addr) < 0) {
        return -1;
    }
    
    if (write_len > 0) {
        if (write(fd, write_buf, write_len) != write_len) {
            return -1;
        }
    }
    
    if (read_len > 0) {
        if (read(fd, read_buf, read_len) != read_len) {
            return -1;
        }
    }
    
    return 0;
}

static void process_key_events(keyboard_context_t *ctx) {
    for (int row = 0; row < KB_MATRIX_ROWS; row++) {
        uint8_t changed = ctx->matrix[row] ^ ctx->prev_matrix[row];
        
        if (changed) {
            for (int col = 0; col < KB_MATRIX_COLS; col++) {
                if (changed & (1 << col)) {
                    uint8_t key = keyboard_scan_to_key(row, col, ctx->modifiers);
                    
                    if (key != KEY_NONE) {
                        key_event_t event;
                        event.key = key;
                        event.modifiers = ctx->modifiers;
                        event.timestamp = get_time_ms();
                        
                        if (ctx->matrix[row] & (1 << col)) {
                            // Key pressed
                            event.state = KB_STATE_PRESSED;
                            ctx->last_key = key;
                            ctx->last_key_time = event.timestamp;
                            
                            // Update modifier state
                            switch (key) {
                                case KEY_SHIFT: ctx->modifiers |= 0x01; break;
                                case KEY_CTRL:  ctx->modifiers |= 0x02; break;
                                case KEY_ALT:   ctx->modifiers |= 0x04; break;
                                case KEY_FN:    ctx->modifiers |= 0x08; break;
                                case KEY_SYM:   ctx->modifiers |= 0x10; break;
                            }
                        } else {
                            // Key released
                            event.state = KB_STATE_RELEASED;
                            
                            // Update modifier state
                            switch (key) {
                                case KEY_SHIFT: ctx->modifiers &= ~0x01; break;
                                case KEY_CTRL:  ctx->modifiers &= ~0x02; break;
                                case KEY_ALT:   ctx->modifiers &= ~0x04; break;
                                case KEY_FN:    ctx->modifiers &= ~0x08; break;
                                case KEY_SYM:   ctx->modifiers &= ~0x10; break;
                            }
                        }
                        
                        // Add to event buffer
                        int next = (ctx->event_tail + 1) % 16;
                        if (next != ctx->event_head) {
                            ctx->event_buffer[ctx->event_tail] = event;
                            ctx->event_tail = next;
                        }
                    }
                }
            }
        }
    }
    
    // Handle key repeat
    if (ctx->config.enable_repeat && ctx->last_key != KEY_NONE) {
        uint32_t now = get_time_ms();
        uint32_t elapsed = now - ctx->last_key_time;
        
        if (elapsed > ctx->config.repeat_delay_ms) {
            uint32_t repeat_elapsed = elapsed - ctx->config.repeat_delay_ms;
            if (repeat_elapsed % ctx->config.repeat_rate_ms < 20) { // Within timing window
                key_event_t event;
                event.key = ctx->last_key;
                event.modifiers = ctx->modifiers;
                event.state = KB_STATE_HELD;
                event.timestamp = now;
                
                int next = (ctx->event_tail + 1) % 16;
                if (next != ctx->event_head) {
                    ctx->event_buffer[ctx->event_tail] = event;
                    ctx->event_tail = next;
                }
            }
        }
    }
}

static void* keyboard_poll_thread(void *arg) {
    keyboard_context_t *ctx = (keyboard_context_t*)arg;
    uint8_t cmd = SB_CMD_KB_SCAN;
    
    while (ctx->running) {
        pthread_mutex_lock(&ctx->lock);
        
        // Save previous state
        memcpy(ctx->prev_matrix, ctx->matrix, KB_MATRIX_ROWS);
        
        // Read keyboard matrix from southbridge
        if (i2c_write_read(ctx->i2c_fd, ctx->i2c_addr, &cmd, 1, 
                          ctx->matrix, KB_MATRIX_ROWS) == 0) {
            process_key_events(ctx);
        }
        
        pthread_mutex_unlock(&ctx->lock);
        
        usleep(10000); // Poll at 100Hz
    }
    
    return NULL;
}

int keyboard_init(keyboard_context_t *ctx, const char *i2c_device, uint8_t i2c_addr) {
    if (!ctx || !i2c_device) {
        return -1;
    }
    
    memset(ctx, 0, sizeof(keyboard_context_t));
    
    // Open I2C device
    ctx->i2c_fd = open(i2c_device, O_RDWR);
    if (ctx->i2c_fd < 0) {
        perror("Failed to open I2C device");
        return -1;
    }
    
    ctx->i2c_addr = i2c_addr;
    
    // Initialize mutex
    if (pthread_mutex_init(&ctx->lock, NULL) != 0) {
        close(ctx->i2c_fd);
        return -1;
    }
    
    // Set default configuration
    ctx->config.enable_repeat = true;
    ctx->config.repeat_delay_ms = 500;
    ctx->config.repeat_rate_ms = 50;
    ctx->config.enable_fn_keys = true;
    ctx->config.enable_sym_keys = true;
    
    // Start polling thread
    ctx->running = true;
    if (pthread_create(&ctx->poll_thread, NULL, keyboard_poll_thread, ctx) != 0) {
        pthread_mutex_destroy(&ctx->lock);
        close(ctx->i2c_fd);
        return -1;
    }
    
    return 0;
}

void keyboard_deinit(keyboard_context_t *ctx) {
    if (!ctx) {
        return;
    }
    
    // Stop polling thread
    ctx->running = false;
    pthread_join(ctx->poll_thread, NULL);
    
    // Clean up
    pthread_mutex_destroy(&ctx->lock);
    close(ctx->i2c_fd);
}

bool keyboard_has_key(keyboard_context_t *ctx) {
    if (!ctx) {
        return false;
    }
    
    pthread_mutex_lock(&ctx->lock);
    bool has_key = (ctx->event_head != ctx->event_tail);
    pthread_mutex_unlock(&ctx->lock);
    
    return has_key;
}

uint8_t keyboard_get_key(keyboard_context_t *ctx) {
    if (!ctx) {
        return KEY_NONE;
    }
    
    uint8_t key = KEY_NONE;
    
    pthread_mutex_lock(&ctx->lock);
    if (ctx->event_head != ctx->event_tail) {
        key = ctx->event_buffer[ctx->event_head].key;
        ctx->event_head = (ctx->event_head + 1) % 16;
    }
    pthread_mutex_unlock(&ctx->lock);
    
    return key;
}

key_event_t keyboard_get_event(keyboard_context_t *ctx) {
    key_event_t event = {0};
    
    if (!ctx) {
        return event;
    }
    
    pthread_mutex_lock(&ctx->lock);
    if (ctx->event_head != ctx->event_tail) {
        event = ctx->event_buffer[ctx->event_head];
        ctx->event_head = (ctx->event_head + 1) % 16;
    }
    pthread_mutex_unlock(&ctx->lock);
    
    return event;
}

void keyboard_clear_buffer(keyboard_context_t *ctx) {
    if (!ctx) {
        return;
    }
    
    pthread_mutex_lock(&ctx->lock);
    ctx->event_head = 0;
    ctx->event_tail = 0;
    pthread_mutex_unlock(&ctx->lock);
}

uint8_t keyboard_scan_to_key(uint8_t row, uint8_t col, uint8_t modifiers) {
    if (row >= KB_MATRIX_ROWS || col >= KB_MATRIX_COLS) {
        return KEY_NONE;
    }
    
    // Check for Fn key first
    if (modifiers & 0x08) {
        return fn_keymap[row][col];
    }
    
    // Check for Shift key
    if (modifiers & 0x01) {
        return shift_keymap[row][col];
    }
    
    // Normal key
    return keymap[row][col];
}

void keyboard_get_matrix(keyboard_context_t *ctx, uint8_t matrix[KB_MATRIX_ROWS]) {
    if (!ctx || !matrix) {
        return;
    }
    
    pthread_mutex_lock(&ctx->lock);
    memcpy(matrix, ctx->matrix, KB_MATRIX_ROWS);
    pthread_mutex_unlock(&ctx->lock);
}

uint8_t keyboard_get_modifiers(keyboard_context_t *ctx) {
    if (!ctx) {
        return 0;
    }
    
    pthread_mutex_lock(&ctx->lock);
    uint8_t mods = ctx->modifiers;
    pthread_mutex_unlock(&ctx->lock);
    
    return mods;
}

bool keyboard_is_shift_pressed(keyboard_context_t *ctx) {
    return (keyboard_get_modifiers(ctx) & 0x01) != 0;
}

bool keyboard_is_ctrl_pressed(keyboard_context_t *ctx) {
    return (keyboard_get_modifiers(ctx) & 0x02) != 0;
}

bool keyboard_is_alt_pressed(keyboard_context_t *ctx) {
    return (keyboard_get_modifiers(ctx) & 0x04) != 0;
}

bool keyboard_is_fn_pressed(keyboard_context_t *ctx) {
    return (keyboard_get_modifiers(ctx) & 0x08) != 0;
}

bool keyboard_is_sym_pressed(keyboard_context_t *ctx) {
    return (keyboard_get_modifiers(ctx) & 0x10) != 0;
}

void keyboard_set_repeat(keyboard_context_t *ctx, bool enable, uint32_t delay_ms, uint32_t rate_ms) {
    if (!ctx) {
        return;
    }
    
    pthread_mutex_lock(&ctx->lock);
    ctx->config.enable_repeat = enable;
    ctx->config.repeat_delay_ms = delay_ms;
    ctx->config.repeat_rate_ms = rate_ms;
    pthread_mutex_unlock(&ctx->lock);
}

const char* keyboard_key_name(uint8_t key) {
    static char buf[16];
    
    switch (key) {
        case KEY_NONE: return "None";
        case KEY_F1: return "F1";
        case KEY_F2: return "F2";
        case KEY_F3: return "F3";
        case KEY_F4: return "F4";
        case KEY_F5: return "F5";
        case KEY_F6: return "F6";
        case KEY_ESCAPE: return "Esc";
        case KEY_TAB: return "Tab";
        case KEY_BACKSPACE: return "Backspace";
        case KEY_RETURN: return "Return";
        case KEY_DELETE: return "Delete";
        case KEY_INSERT: return "Insert";
        case KEY_HOME: return "Home";
        case KEY_END: return "End";
        case KEY_PAGEUP: return "PageUp";
        case KEY_PAGEDOWN: return "PageDown";
        case KEY_BREAK: return "Break";
        case KEY_UP: return "Up";
        case KEY_DOWN: return "Down";
        case KEY_LEFT: return "Left";
        case KEY_RIGHT: return "Right";
        case KEY_SHIFT: return "Shift";
        case KEY_CTRL: return "Ctrl";
        case KEY_ALT: return "Alt";
        case KEY_FN: return "Fn";
        case KEY_SYM: return "Sym";
        default:
            if (key >= 32 && key < 127) {
                snprintf(buf, sizeof(buf), "%c", key);
            } else {
                snprintf(buf, sizeof(buf), "0x%02X", key);
            }
            return buf;
    }
}