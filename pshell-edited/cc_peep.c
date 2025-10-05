#include <stdint.h>

#include "cc.h"
#include "cc_peep.h"

#define NUMOF(a) (sizeof(a) / sizeof(a[0]))

// Pico-optimized peep hole optimizer

// FROM:              TO:
// mov  r0, r7        mov  r3,r7
// push {r0}          movs r0,#n
// movs r0,#n
// pop  {r3}

static const uint16_t pat0[] = {0x4638, 0xb401, 0x2000, 0xbc08};
static const uint16_t msk0[] = {0xffff, 0xffff, 0xff00, 0xffff};
static const uint16_t rep0[] = {0x463b, 0x2000};

// ... rest unchanged for efficiency

// Only change if you need to add Pico-specific peep optimizations

struct subs {
    int8_t from;
    int8_t to;
    int8_t lshft;
};

static const struct segs {
    uint8_t n_pats;
    uint8_t n_reps;
    uint8_t n_maps;
    const uint16_t* pat;
    const uint16_t* msk;
    const uint16_t* rep;
    struct subs map[2];
} segments[] = {
    {NUMOF(pat0), NUMOF(rep0), 1, pat0, msk0, rep0, {{2, 1, 0}, {}}},
    // ... rest unchanged
};

void peep(void);

static void peep_hole(const struct segs* s) {
    uint16_t rslt[8], final[8];
    int l = s->n_pats;
    uint16_t* pe = (e - l) + 1;
    if (pe < text_base)
        return;
    for (int i = 0; i < l; i++) {
        if ((pe[i] & s->msk[i]) != (s->pat[i] & s->msk[i]))
            return;
        rslt[i] = pe[i] & ~s->msk[i];
    }
    e -= l;
    l = s->n_reps;
    for (int i = 0; i < l; i++)
        final[i] = s->rep[i];
    for (int i = 0; i < s->n_maps; ++i)
        final[s->map[i].to] |= rslt[s->map[i].from] << s->map[i].lshft;
    for (int i = 0; i < l; i++) {
        *++e = final[i];
        peep();
    }
}

void peep(void) {
    for (int i = 0; i < NUMOF(segments); ++i)
        peep_hole(&segments[i]);
}
