#ifndef NABTOSHELL_PROMPT_PROTOCOL_H_
#define NABTOSHELL_PROMPT_PROTOCOL_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "nabtoshell_prompt.h"

typedef struct {
    char* instance_id;
    char* decision;
    char* keys;
} nabtoshell_prompt_resolve_message;

uint8_t* nabtoshell_prompt_protocol_encode_present(
    const nabtoshell_prompt_instance* instance,
    size_t* out_len);

uint8_t* nabtoshell_prompt_protocol_encode_update(
    const nabtoshell_prompt_instance* instance,
    size_t* out_len);

uint8_t* nabtoshell_prompt_protocol_encode_gone(
    const char* instance_id,
    size_t* out_len);

bool nabtoshell_prompt_protocol_decode_resolve(
    const uint8_t* framed_data,
    size_t framed_len,
    nabtoshell_prompt_resolve_message* out);

void nabtoshell_prompt_protocol_free_resolve(
    nabtoshell_prompt_resolve_message* message);

#endif
