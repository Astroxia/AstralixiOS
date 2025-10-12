#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>

// Keyboard key codes matching PicoCalc layout
#define KEY_NONE        0x00

// Function keys
#define KEY_F1          0x01
#define KEY_F2          0x02
#define KEY_F3          0x03
#define KEY_F4          0x04
#define KEY_F5          0x05
#define KEY_F6          0x06

// Special keys
#define KEY_ESCAPE      0x1B
#define KEY_TAB         0x09
#define KEY_BACKSPACE   0x08
#define KEY_RETURN      0x0D
#define KEY_DELETE      0x7F
#define KEY_INSERT      0x80
#define KEY_HOME        0x81
#define KEY_END         0x82
#define KEY_PAGEUP      0x83
#define KEY_PAGEDOWN    0x84
#define KEY_BREAK       0x85

// Arrow keys
#define KEY_UP          0x86
#define KEY_DOWN        0x87
#define KEY_LEFT        0x88
#define KEY_RIGHT       0x89

// Modifier keys
#define KEY_SHIFT       0x8A
#define KEY_CTRL        0x8B
#define KEY_ALT         0x8C
#define KEY_FN          0x8D
#define KEY_SYM         0x8E

// Keyboard matrix size (PicoCalc specific)
#define KB_MATRIX_ROWS  7
#define KB_MATRIX_COLS  8

// Keyboard states
typedef enum {
    KB_STATE_IDLE,
    KB_STATE_PRESSED,
    KB_STATE_HELD,
    KB_STATE_RELEASED
} keyboard_state_t;

// Key event structure
typedef struct {
    uint8_t key;
    uint8_t modifiers;
    keyboard_state_t state;
    uint32_t timestamp;
} key_event_t;

// Keyboard configuration
typedef struct {
    bool enable_repeat;
    uint32_t repeat_delay_ms;
    uint32_t repeat_rate_ms;
    bool enable_fn_keys;
    bool enable_sym_keys;
} keyboard_config_t;

// Keyboard context structure
typedef struct {
    int i2c_fd;                      // I2C file descriptor
    uint8_t i2c_addr;                // I2C address of southbridge
    pthread_t poll_thread;           // Polling thread
    pthread_mutex_t lock;            // Thread safety
    bool running;                    // Thread control
    
    uint8_t matrix[KB_MATRIX_ROWS]; // Current matrix state
    uint8_t prev_matrix[KB_MATRIX_ROWS]; // Previous state
    
    key_event_t event_buffer[16];   // Event queue
    int event_head;
    int event_tail;
    
    keyboard_config_t config;       // Configuration
    
    uint8_t modifiers;              // Current modifier state
    uint32_t last_key_time;         // For repeat handling
    uint8_t last_key;               // Last pressed key
} keyboard_context_t;

// Function prototypes
int keyboard_init(keyboard_context_t *ctx, const char *i2c_device, uint8_t i2c_addr);
void keyboard_deinit(keyboard_context_t *ctx);

// Key operations
bool keyboard_has_key(keyboard_context_t *ctx);
uint8_t keyboard_get_key(keyboard_context_t *ctx);
key_event_t keyboard_get_event(keyboard_context_t *ctx);
void keyboard_clear_buffer(keyboard_context_t *ctx);

// Configuration
void keyboard_set_config(keyboard_context_t *ctx, const keyboard_config_t *config);
void keyboard_get_config(keyboard_context_t *ctx, keyboard_config_t *config);

// Modifier state
uint8_t keyboard_get_modifiers(keyboard_context_t *ctx);
bool keyboard_is_shift_pressed(keyboard_context_t *ctx);
bool keyboard_is_ctrl_pressed(keyboard_context_t *ctx);
bool keyboard_is_alt_pressed(keyboard_context_t *ctx);
bool keyboard_is_fn_pressed(keyboard_context_t *ctx);
bool keyboard_is_sym_pressed(keyboard_context_t *ctx);

// Matrix access (for advanced use)
void keyboard_get_matrix(keyboard_context_t *ctx, uint8_t matrix[KB_MATRIX_ROWS]);
uint8_t keyboard_scan_to_key(uint8_t row, uint8_t col, uint8_t modifiers);

// Utility functions
const char* keyboard_key_name(uint8_t key);
void keyboard_set_repeat(keyboard_context_t *ctx, bool enable, uint32_t delay_ms, uint32_t rate_ms);

#endif // KEYBOARD_H