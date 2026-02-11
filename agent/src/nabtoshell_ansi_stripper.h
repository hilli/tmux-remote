#ifndef NABTOSHELL_ANSI_STRIPPER_H
#define NABTOSHELL_ANSI_STRIPPER_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
    ANSI_STATE_GROUND,
    ANSI_STATE_ESCAPE,
    ANSI_STATE_ESCAPE_INTERMEDIATE,
    ANSI_STATE_CSI,
    ANSI_STATE_OSC,
    ANSI_STATE_OSC_ESCAPE
} nabtoshell_ansi_state;

typedef struct {
    nabtoshell_ansi_state state;
    uint8_t pending_utf8[4];
    int pending_utf8_len;
} nabtoshell_ansi_stripper;

void nabtoshell_ansi_stripper_init(nabtoshell_ansi_stripper *s);
void nabtoshell_ansi_stripper_reset(nabtoshell_ansi_stripper *s);

// Feed raw bytes, write stripped UTF-8 text to out.
// Returns number of bytes written to out.
// out must be large enough (worst case: in_len + pending_utf8_len).
size_t nabtoshell_ansi_stripper_feed(nabtoshell_ansi_stripper *s,
                                      const uint8_t *in, size_t in_len,
                                      uint8_t *out, size_t out_cap);

#endif
