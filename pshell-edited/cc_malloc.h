#pragma once

// Use stdint.h for type safety
#include <stdint.h>

void* cc_malloc(int nbytes, int user, int zero);
void cc_free(void* m, int user);
void cc_free_all(void);
