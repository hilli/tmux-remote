#ifndef NABTOSHELL_CLIENT_CONFIG_H_
#define NABTOSHELL_CLIENT_CONFIG_H_

#include "nabtoshell_client.h"

#include <stdbool.h>

bool nabtoshell_config_init(struct nabtoshell_client_config* config);
void nabtoshell_config_deinit(struct nabtoshell_client_config* config);

bool nabtoshell_config_ensure_dirs(struct nabtoshell_client_config* config);
bool nabtoshell_config_load_devices(struct nabtoshell_client_config* config);
bool nabtoshell_config_save_devices(struct nabtoshell_client_config* config);
bool nabtoshell_config_add_device(struct nabtoshell_client_config* config,
                                  const struct nabtoshell_device_bookmark* device);

struct nabtoshell_device_bookmark* nabtoshell_config_find_device(
    struct nabtoshell_client_config* config, const char* name);

bool nabtoshell_config_load_or_create_key(struct nabtoshell_client_config* config,
                                          NabtoClient* client, char** privateKey);

#endif
