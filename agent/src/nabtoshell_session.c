#include "nabtoshell_session.h"

#include <string.h>

void nabtoshell_session_map_init(struct nabtoshell_session_map* map)
{
    memset(map, 0, sizeof(struct nabtoshell_session_map));
}

bool nabtoshell_session_set(struct nabtoshell_session_map* map,
                            NabtoDeviceConnectionRef ref,
                            const char* sessionName,
                            uint16_t cols, uint16_t rows)
{
    /* First check if this connection already has an entry */
    for (int i = 0; i < NABTOSHELL_MAX_SESSIONS; i++) {
        if (map->entries[i].valid && map->entries[i].connectionRef == ref) {
            strncpy(map->entries[i].sessionName, sessionName,
                    NABTOSHELL_SESSION_NAME_MAX - 1);
            map->entries[i].cols = cols;
            map->entries[i].rows = rows;
            return true;
        }
    }

    /* Find a free slot */
    for (int i = 0; i < NABTOSHELL_MAX_SESSIONS; i++) {
        if (!map->entries[i].valid) {
            map->entries[i].connectionRef = ref;
            strncpy(map->entries[i].sessionName, sessionName,
                    NABTOSHELL_SESSION_NAME_MAX - 1);
            map->entries[i].sessionName[NABTOSHELL_SESSION_NAME_MAX - 1] = '\0';
            map->entries[i].cols = cols;
            map->entries[i].rows = rows;
            map->entries[i].valid = true;
            return true;
        }
    }

    return false;
}

struct nabtoshell_session_entry* nabtoshell_session_find(
    struct nabtoshell_session_map* map,
    NabtoDeviceConnectionRef ref)
{
    for (int i = 0; i < NABTOSHELL_MAX_SESSIONS; i++) {
        if (map->entries[i].valid && map->entries[i].connectionRef == ref) {
            return &map->entries[i];
        }
    }
    return NULL;
}

void nabtoshell_session_remove(struct nabtoshell_session_map* map,
                               NabtoDeviceConnectionRef ref)
{
    for (int i = 0; i < NABTOSHELL_MAX_SESSIONS; i++) {
        if (map->entries[i].valid && map->entries[i].connectionRef == ref) {
            memset(&map->entries[i], 0, sizeof(struct nabtoshell_session_entry));
        }
    }
}
