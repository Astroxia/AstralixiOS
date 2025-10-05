#pragma once

// Use Pico standard types for compatibility
#include <stdint.h>
#include <stdbool.h>

int cc(int mode, int argc, char* argv[]);

#define UDATA __attribute__((section(".ccudata")))

__attribute__((__noreturn__)) void run_fatal(const char* fmt, ...);
__attribute__((__noreturn__)) void fatal_func(const char* func, int lne, const char* fmt, ...);

// fatal error message and exit
#define fatal(fmt, ...) fatal_func(__FUNCTION__, __LINE__, fmt, ##__VA_ARGS__)

extern uint16_t* e;
extern const uint16_t* text_base;
extern const uint32_t prog_space;
extern const uint32_t data_space;

// Add Pico-specific externs if needed by AstralixiOS
extern void* cc_malloc(int nbytes, int user, int zero);
extern void cc_free(void* m, int user);
extern void cc_free_all(void);
