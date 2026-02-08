#ifndef NABTOSHELL_CLIENT_COAP_H_
#define NABTOSHELL_CLIENT_COAP_H_

#include <nabto/nabto_client.h>
#include <stdbool.h>
#include <stdint.h>

bool nabtoshell_coap_attach(NabtoClientConnection* conn, NabtoClient* client,
                            const char* session, uint16_t cols, uint16_t rows,
                            bool create);

bool nabtoshell_coap_create_session(NabtoClientConnection* conn,
                                    NabtoClient* client,
                                    const char* session, uint16_t cols,
                                    uint16_t rows, const char* command);

bool nabtoshell_coap_resize(NabtoClientConnection* conn, NabtoClient* client,
                            uint16_t cols, uint16_t rows);

bool nabtoshell_coap_pair_password_invite(NabtoClientConnection* conn,
                                          NabtoClient* client,
                                          const char* username);

/* sessions list result */
#define NABTOSHELL_CLIENT_MAX_SESSIONS 32

struct nabtoshell_client_session_info {
    char name[64];
    uint16_t cols;
    uint16_t rows;
    int attached;
};

struct nabtoshell_client_sessions_list {
    struct nabtoshell_client_session_info sessions[NABTOSHELL_CLIENT_MAX_SESSIONS];
    int count;
};

bool nabtoshell_coap_list_sessions(NabtoClientConnection* conn,
                                   NabtoClient* client,
                                   struct nabtoshell_client_sessions_list* list);

struct nabtoshell_client_status_info {
    char version[32];
    int activeSessions;
    uint64_t uptimeSeconds;
};

bool nabtoshell_coap_get_status(NabtoClientConnection* conn,
                                NabtoClient* client,
                                struct nabtoshell_client_status_info* info);

#endif
