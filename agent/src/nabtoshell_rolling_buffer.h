#ifndef NABTOSHELL_ROLLING_BUFFER_H
#define NABTOSHELL_ROLLING_BUFFER_H

#include <stddef.h>

typedef struct {
    char *data;
    size_t capacity;
    size_t len;
    size_t total_appended;
} nabtoshell_rolling_buffer;

void nabtoshell_rolling_buffer_init(nabtoshell_rolling_buffer *b, size_t capacity);
void nabtoshell_rolling_buffer_free(nabtoshell_rolling_buffer *b);
void nabtoshell_rolling_buffer_reset(nabtoshell_rolling_buffer *b);
void nabtoshell_rolling_buffer_append(nabtoshell_rolling_buffer *b, const char *text, size_t len);

// Returns pointer to the last `count` bytes (or fewer if buffer is shorter).
// Sets *out_len to actual length returned.
const char *nabtoshell_rolling_buffer_tail(const nabtoshell_rolling_buffer *b, size_t count, size_t *out_len);

#endif
