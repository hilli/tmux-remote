#include "tmuxremote_devices.h"
#include "tmuxremote_client.h"
#include "tmuxremote_client_config.h"
#include "tmuxremote_client_coap.h"
#include "tmuxremote_client_util.h"

#include <nabto/nabto_client.h>

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CONNECT_TIMEOUT_MS 2000

struct agent_probe_ctx {
    struct tmuxremote_device_bookmark* dev;
    char* privateKey;
    bool online;
    int activeSessions;
};

static void* agent_probe_thread(void* arg)
{
    struct agent_probe_ctx* ctx = arg;
    ctx->online = false;
    ctx->activeSessions = 0;

    NabtoClient* client = nabto_client_new();
    if (client == NULL) {
        return NULL;
    }

    NabtoClientConnection* conn = nabto_client_connection_new(client);
    if (conn == NULL) {
        nabto_client_free(client);
        return NULL;
    }

    char* optionsJson = tmuxremote_build_connection_options(
        ctx->dev->productId, ctx->dev->deviceId, ctx->privateKey, ctx->dev->sct);
    if (optionsJson == NULL) {
        nabto_client_connection_free(conn);
        nabto_client_free(client);
        return NULL;
    }

    NabtoClientError ec = nabto_client_connection_set_options(conn, optionsJson);
    free(optionsJson);
    if (ec != NABTO_CLIENT_EC_OK) {
        nabto_client_connection_free(conn);
        nabto_client_free(client);
        return NULL;
    }

    /* Connect with timeout */
    NabtoClientFuture* future = nabto_client_future_new(client);
    nabto_client_connection_connect(conn, future);
    ec = nabto_client_future_timed_wait(future, CONNECT_TIMEOUT_MS);
    nabto_client_future_free(future);

    if (ec != NABTO_CLIENT_EC_OK) {
        nabto_client_connection_free(conn);
        nabto_client_free(client);
        return NULL;
    }

    /* Get status */
    struct tmuxremote_client_status_info status;
    if (tmuxremote_coap_get_status(conn, client, &status)) {
        ctx->online = true;
        ctx->activeSessions = status.activeSessions;
    }

    /* Close connection */
    future = nabto_client_future_new(client);
    nabto_client_connection_close(conn, future);
    nabto_client_future_timed_wait(future, 1000);
    nabto_client_future_free(future);

    nabto_client_connection_free(conn);
    nabto_client_free(client);
    return NULL;
}

int tmuxremote_cmd_devices(int argc, char** argv)
{
    bool noProbe = false;
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--no-probe") == 0) {
            noProbe = true;
        }
    }

    struct tmuxremote_client_config config;
    if (!tmuxremote_config_init(&config)) {
        return 1;
    }
    tmuxremote_config_load_devices(&config);

    if (config.deviceCount == 0) {
        printf("No saved agents.\n");
        printf("Use 'tmux-remote pair <pairing-string>' to pair with an agent.\n");
        tmuxremote_config_deinit(&config);
        return 0;
    }

    int count = config.deviceCount;
    struct agent_probe_ctx ctxs[TMUXREMOTE_MAX_DEVICES];
    pthread_t threads[TMUXREMOTE_MAX_DEVICES];
    bool threadStarted[TMUXREMOTE_MAX_DEVICES];

    memset(ctxs, 0, sizeof(ctxs));
    memset(threadStarted, 0, sizeof(threadStarted));

    if (!noProbe) {
        /* Load private key (shared by all probes) */
        NabtoClient* tmpClient = nabto_client_new();
        if (tmpClient == NULL) {
            tmuxremote_config_deinit(&config);
            return 1;
        }
        char* privateKey = NULL;
        if (!tmuxremote_config_load_or_create_key(&config, tmpClient, &privateKey)) {
            nabto_client_free(tmpClient);
            tmuxremote_config_deinit(&config);
            return 1;
        }
        nabto_client_free(tmpClient);

        /* Launch probe threads in parallel */
        for (int i = 0; i < count; i++) {
            ctxs[i].dev = &config.devices[i];
            ctxs[i].privateKey = privateKey;
            if (pthread_create(&threads[i], NULL, agent_probe_thread, &ctxs[i]) == 0) {
                threadStarted[i] = true;
            }
        }

        /* Wait for all threads */
        for (int i = 0; i < count; i++) {
            if (threadStarted[i]) {
                pthread_join(threads[i], NULL);
            }
        }

        free(privateKey);
    }

    /* Print results */
    printf("Saved agents:\n");
    if (noProbe) {
        printf("       %-20s %s\n", "NAME", "FINGERPRINT");
    } else {
        printf("       %-20s %-14s %s\n", "NAME", "FINGERPRINT", "STATUS");
    }
    for (int i = 0; i < count; i++) {
        char fpShort[18] = {0};
        if (config.devices[i].fingerprint[0] != '\0') {
            snprintf(fpShort, sizeof(fpShort), "%.12s...",
                     config.devices[i].fingerprint);
        }

        if (noProbe) {
            printf("  [%2d] %-20s %s\n",
                   i, config.devices[i].name, fpShort);
        } else {
            char statusStr[32];
            if (!threadStarted[i] || !ctxs[i].online) {
                snprintf(statusStr, sizeof(statusStr), "offline");
            } else if (ctxs[i].activeSessions == 1) {
                snprintf(statusStr, sizeof(statusStr), "1 session");
            } else {
                snprintf(statusStr, sizeof(statusStr), "%d sessions",
                         ctxs[i].activeSessions);
            }

            printf("  [%2d] %-20s %-14s %s\n",
                   i, config.devices[i].name, fpShort, statusStr);
        }
    }

    tmuxremote_config_deinit(&config);
    return 0;
}
