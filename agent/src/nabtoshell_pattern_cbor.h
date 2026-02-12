#ifndef NABTOSHELL_PATTERN_CBOR_H
#define NABTOSHELL_PATTERN_CBOR_H

#include "nabtoshell_pattern_matcher.h"
#include <stddef.h>
#include <stdbool.h>

/*
 * Encode a pattern match as a length-prefixed CBOR message:
 *   [4 bytes: big-endian uint32 payload length][N bytes: CBOR map]
 *
 * CBOR map: {type, pattern_id, pattern_type, prompt?, actions}
 *
 * Returns malloc'd buffer or NULL. Caller must free.
 */
uint8_t *nabtoshell_pattern_cbor_encode_match(const nabtoshell_pattern_match *match,
                                               size_t *outLen);

/*
 * Encode a pattern dismiss as a length-prefixed CBOR message:
 *   [4 bytes: big-endian uint32 payload length][CBOR: {"type":"pattern_dismiss"}]
 *
 * Returns malloc'd buffer or NULL. Caller must free.
 */
uint8_t *nabtoshell_pattern_cbor_encode_dismiss(size_t *outLen);

/*
 * Decode a length-prefixed CBOR pattern message.
 *
 * If the message is a pattern_match, returns a newly allocated match.
 * If the message is a pattern_dismiss, returns NULL and sets *is_dismiss = true.
 * On error, returns NULL and *is_dismiss = false.
 *
 * data/len should include the 4-byte length prefix.
 * Caller must free the returned match with nabtoshell_pattern_match_free().
 */
nabtoshell_pattern_match *nabtoshell_pattern_cbor_decode(const uint8_t *data,
                                                          size_t len,
                                                          bool *is_dismiss);

#endif
