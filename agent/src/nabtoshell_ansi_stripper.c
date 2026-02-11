#include "nabtoshell_ansi_stripper.h"
#include <string.h>

void nabtoshell_ansi_stripper_init(nabtoshell_ansi_stripper *s)
{
    s->state = ANSI_STATE_GROUND;
    s->pending_utf8_len = 0;
}

void nabtoshell_ansi_stripper_reset(nabtoshell_ansi_stripper *s)
{
    s->state = ANSI_STATE_GROUND;
    s->pending_utf8_len = 0;
}

// Returns how many trailing bytes form an incomplete UTF-8 sequence.
static int incomplete_utf8_suffix(const uint8_t *buf, size_t len)
{
    if (len == 0) return 0;

    int limit = len < 3 ? (int)len : 3;
    for (int i = 1; i <= limit; i++) {
        uint8_t byte = buf[len - i];

        if (byte < 0x80) {
            return 0;  // ASCII: everything complete
        }
        if (byte >= 0xC0) {
            // Multi-byte start byte found
            int expected;
            if (byte < 0xE0) expected = 2;
            else if (byte < 0xF0) expected = 3;
            else expected = 4;
            return i < expected ? i : 0;
        }
        // 0x80-0xBF: continuation byte, keep walking back
    }
    return 0;
}

size_t nabtoshell_ansi_stripper_feed(nabtoshell_ansi_stripper *s,
                                      const uint8_t *in, size_t in_len,
                                      uint8_t *out, size_t out_cap)
{
    size_t out_len = 0;

    // Prepend pending UTF-8 bytes from previous chunk
    if (s->pending_utf8_len > 0) {
        for (int i = 0; i < s->pending_utf8_len && out_len < out_cap; i++) {
            out[out_len++] = s->pending_utf8[i];
        }
        s->pending_utf8_len = 0;
    }

    for (size_t i = 0; i < in_len; i++) {
        uint8_t byte = in[i];

        switch (s->state) {
        case ANSI_STATE_GROUND:
            if (byte == 0x1B) {
                s->state = ANSI_STATE_ESCAPE;
            } else if (byte == 0x0A || byte == 0x0D) {
                if (out_len < out_cap) out[out_len++] = 0x0A;
            } else if (byte == 0x09) {
                // TAB -> 4 spaces
                for (int t = 0; t < 4 && out_len < out_cap; t++)
                    out[out_len++] = 0x20;
            } else if (byte >= 0x20) {
                if (out_len < out_cap) out[out_len++] = byte;
            }
            // else: strip C0 controls
            break;

        case ANSI_STATE_ESCAPE:
            if (byte == 0x5B) {
                s->state = ANSI_STATE_CSI;
            } else if (byte == 0x5D) {
                s->state = ANSI_STATE_OSC;
            } else if (byte >= 0x20 && byte <= 0x2F) {
                s->state = ANSI_STATE_ESCAPE_INTERMEDIATE;
            } else {
                s->state = ANSI_STATE_GROUND;
            }
            break;

        case ANSI_STATE_ESCAPE_INTERMEDIATE:
            s->state = ANSI_STATE_GROUND;
            break;

        case ANSI_STATE_CSI:
            if (byte >= 0x40 && byte <= 0x7E) {
                switch (byte) {
                case 0x41: // A - Cursor Up
                case 0x42: // B - Cursor Down
                case 0x45: // E - Cursor Next Line
                case 0x46: // F - Cursor Previous Line
                case 0x48: // H - Cursor Position
                case 0x64: // d - Vertical Position Absolute
                case 0x66: // f - Horizontal Vertical Position
                    if (out_len < out_cap) out[out_len++] = 0x0A;
                    break;
                case 0x43: // C - Cursor Forward
                case 0x47: // G - Cursor Horizontal Absolute
                    if (out_len < out_cap) out[out_len++] = 0x20;
                    break;
                default:
                    break;
                }
                s->state = ANSI_STATE_GROUND;
            }
            break;

        case ANSI_STATE_OSC:
            if (byte == 0x07) {
                s->state = ANSI_STATE_GROUND;
            } else if (byte == 0x1B) {
                s->state = ANSI_STATE_OSC_ESCAPE;
            }
            break;

        case ANSI_STATE_OSC_ESCAPE:
            if (byte == 0x5C) {
                s->state = ANSI_STATE_GROUND;
            } else if (byte == 0x5B) {
                s->state = ANSI_STATE_CSI;
            } else if (byte == 0x5D) {
                s->state = ANSI_STATE_OSC;
            } else {
                s->state = ANSI_STATE_GROUND;
            }
            break;
        }
    }

    // Save incomplete UTF-8 at end of output
    int trailing = incomplete_utf8_suffix(out, out_len);
    if (trailing > 0) {
        for (int i = 0; i < trailing; i++) {
            s->pending_utf8[i] = out[out_len - trailing + i];
        }
        s->pending_utf8_len = trailing;
        out_len -= trailing;
    }

    return out_len;
}
