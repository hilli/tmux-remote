#ifndef NABTOSHELL_CLIENT_H_
#define NABTOSHELL_CLIENT_H_

#include <nabto/nabto_client.h>
#include <stdbool.h>
#include <stdint.h>

#define NABTOSHELL_CLIENT_VERSION "0.1.0"
#define NABTOSHELL_MAX_DEVICES 32
#define NABTOSHELL_MAX_NAME_LEN 64

struct nabtoshell_device_bookmark {
    char name[NABTOSHELL_MAX_NAME_LEN];
    char productId[NABTOSHELL_MAX_NAME_LEN];
    char deviceId[NABTOSHELL_MAX_NAME_LEN];
    char fingerprint[128];
    char sct[NABTOSHELL_MAX_NAME_LEN];
};

struct nabtoshell_client_config {
    char* configDir;
    char* keyFile;
    char* devicesFile;
    struct nabtoshell_device_bookmark devices[NABTOSHELL_MAX_DEVICES];
    int deviceCount;
};

struct nabtoshell_pairing_info {
    char productId[NABTOSHELL_MAX_NAME_LEN];
    char deviceId[NABTOSHELL_MAX_NAME_LEN];
    char username[NABTOSHELL_MAX_NAME_LEN];
    char password[NABTOSHELL_MAX_NAME_LEN];
    char sct[NABTOSHELL_MAX_NAME_LEN];
};

#endif
