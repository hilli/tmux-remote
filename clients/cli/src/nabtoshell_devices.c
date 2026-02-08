#include "nabtoshell_devices.h"
#include "nabtoshell_client.h"
#include "nabtoshell_client_config.h"

#include <stdio.h>

int nabtoshell_cmd_devices(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    struct nabtoshell_client_config config;
    if (!nabtoshell_config_init(&config)) {
        return 1;
    }
    nabtoshell_config_load_devices(&config);

    if (config.deviceCount == 0) {
        printf("No saved devices.\n");
        printf("Use 'nabtoshell pair <pairing-string>' to pair with a device.\n");
    } else {
        printf("Saved devices:\n");
        printf("  %-20s %-14s %-14s %s\n", "NAME", "PRODUCT", "DEVICE", "FINGERPRINT");
        for (int i = 0; i < config.deviceCount; i++) {
            char fpShort[18] = {0};
            if (config.devices[i].fingerprint[0] != '\0') {
                snprintf(fpShort, sizeof(fpShort), "%.12s...",
                         config.devices[i].fingerprint);
            }
            printf("  %-20s %-14s %-14s %s\n",
                   config.devices[i].name,
                   config.devices[i].productId,
                   config.devices[i].deviceId,
                   fpShort);
        }
    }

    nabtoshell_config_deinit(&config);
    return 0;
}
