#ifndef NABTOSHELL_CLIENT_UTIL_H_
#define NABTOSHELL_CLIENT_UTIL_H_

#include "nabtoshell_client.h"

#include <stdbool.h>
#include <stdint.h>
#include <termios.h>

bool nabtoshell_parse_pairing_string(const char* str,
                                     struct nabtoshell_pairing_info* info);

bool nabtoshell_terminal_get_size(uint16_t* cols, uint16_t* rows);
bool nabtoshell_terminal_set_raw(struct termios* saved);
bool nabtoshell_terminal_restore(const struct termios* saved);

#endif
