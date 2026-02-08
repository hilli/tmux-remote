#ifndef NABTOSHELL_INIT_H_
#define NABTOSHELL_INIT_H_

#include <stdbool.h>

bool nabtoshell_do_init(const char* homeDir, const char* productId,
                        const char* deviceId);

bool nabtoshell_do_demo_init(const char* homeDir, const char* productId,
                             const char* deviceId);

bool nabtoshell_do_add_user(const char* homeDir, const char* username);

bool nabtoshell_do_remove_user(const char* homeDir, const char* username);

#endif
