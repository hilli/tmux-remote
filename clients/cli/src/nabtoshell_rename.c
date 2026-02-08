#include "nabtoshell_rename.h"
#include "nabtoshell_client.h"
#include "nabtoshell_client_config.h"

#include <stdio.h>
#include <string.h>

int nabtoshell_cmd_rename(int argc, char** argv)
{
    if (argc < 3) {
        printf("Usage: nabtoshell rename <current-name> <new-name>\n");
        return 1;
    }

    const char* currentName = argv[1];
    const char* newName = argv[2];

    struct nabtoshell_client_config config;
    if (!nabtoshell_config_init(&config)) {
        return 1;
    }
    nabtoshell_config_load_devices(&config);

    struct nabtoshell_device_bookmark* dev =
        nabtoshell_config_find_device(&config, currentName);
    if (dev == NULL) {
        printf("Device '%s' not found.\n", currentName);
        nabtoshell_config_deinit(&config);
        return 1;
    }

    strncpy(dev->name, newName, sizeof(dev->name) - 1);
    dev->name[sizeof(dev->name) - 1] = '\0';

    if (!nabtoshell_config_save_devices(&config)) {
        printf("Failed to save devices.\n");
        nabtoshell_config_deinit(&config);
        return 1;
    }

    printf("Renamed '%s' to '%s'.\n", currentName, newName);

    nabtoshell_config_deinit(&config);
    return 0;
}
