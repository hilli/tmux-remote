#include "nabtoshell_connect.h"
#include "nabtoshell_client.h"
#include "nabtoshell_client_config.h"
#include "nabtoshell_client_util.h"
#include "nabtoshell_client_coap.h"

#include <nabto/nabto_client.h>

#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <unistd.h>

#define READ_BUFFER_SIZE 4096

struct reader_thread_ctx {
    NabtoClient* client;
    NabtoClientStream* stream;
    atomic_bool* done;
};

static volatile sig_atomic_t sigwinch_received = 0;

static void sigwinch_handler(int sig)
{
    (void)sig;
    sigwinch_received = 1;
}

static void* stream_reader_thread(void* arg)
{
    struct reader_thread_ctx* ctx = arg;
    uint8_t buf[READ_BUFFER_SIZE];
    size_t readLen = 0;

    while (!atomic_load(ctx->done)) {
        NabtoClientFuture* fut = nabto_client_future_new(ctx->client);
        nabto_client_stream_read_some(ctx->stream, fut, buf, sizeof(buf), &readLen);
        NabtoClientError ec = nabto_client_future_wait(fut);
        nabto_client_future_free(fut);

        if (ec == NABTO_CLIENT_EC_EOF || ec != NABTO_CLIENT_EC_OK) {
            break;
        }

        if (readLen > 0) {
            ssize_t w = write(STDOUT_FILENO, buf, readLen);
            (void)w;
        }
    }

    atomic_store(ctx->done, true);
    return NULL;
}

int nabtoshell_cmd_connect(int argc, char** argv)
{
    if (argc < 2) {
        printf("Usage: nabtoshell connect <device-name> [-s session] [--new command]\n");
        return 1;
    }

    const char* deviceName = argv[1];
    const char* sessionName = NULL;
    const char* newCommand = NULL;
    bool createNew = false;

    /* Parse options */
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-s") == 0 && i + 1 < argc) {
            sessionName = argv[i + 1];
            i++;
        } else if (strcmp(argv[i], "--new") == 0 && i + 1 < argc) {
            newCommand = argv[i + 1];
            createNew = true;
            i++;
        } else if (strcmp(argv[i], "--new") == 0 && i + 1 >= argc) {
            /* --new without argument: create with default shell */
            createNew = true;
        }
    }

    /* Load config */
    struct nabtoshell_client_config config;
    if (!nabtoshell_config_init(&config)) {
        return 1;
    }
    nabtoshell_config_load_devices(&config);

    struct nabtoshell_device_bookmark* dev =
        nabtoshell_config_find_device(&config, deviceName);
    if (dev == NULL) {
        printf("Device '%s' not found. Run 'nabtoshell devices' to see saved devices.\n",
               deviceName);
        nabtoshell_config_deinit(&config);
        return 1;
    }

    /* Create client */
    NabtoClient* client = nabto_client_new();
    if (client == NULL) {
        printf("Failed to create Nabto client\n");
        nabtoshell_config_deinit(&config);
        return 1;
    }

    /* Load private key */
    char* privateKey = NULL;
    if (!nabtoshell_config_load_or_create_key(&config, client, &privateKey)) {
        nabto_client_free(client);
        nabtoshell_config_deinit(&config);
        return 1;
    }

    /* Create and configure connection */
    NabtoClientConnection* conn = nabto_client_connection_new(client);
    if (conn == NULL) {
        free(privateKey);
        nabto_client_free(client);
        nabtoshell_config_deinit(&config);
        return 1;
    }

    char* optionsJson = nabtoshell_build_connection_options(
        dev->productId, dev->deviceId, privateKey, dev->sct);
    free(privateKey);
    if (optionsJson == NULL) {
        nabto_client_connection_free(conn);
        nabto_client_free(client);
        nabtoshell_config_deinit(&config);
        return 1;
    }

    NabtoClientError ec = nabto_client_connection_set_options(conn, optionsJson);
    free(optionsJson);
    if (ec != NABTO_CLIENT_EC_OK) {
        printf("Failed to set connection options: %s\n",
               nabto_client_error_get_message(ec));
        nabto_client_connection_free(conn);
        nabto_client_free(client);
        nabtoshell_config_deinit(&config);
        return 1;
    }

    /* Connect */
    NabtoClientFuture* future = nabto_client_future_new(client);
    nabto_client_connection_connect(conn, future);
    ec = nabto_client_future_wait(future);
    nabto_client_future_free(future);

    if (ec != NABTO_CLIENT_EC_OK) {
        printf("Failed to connect to '%s': %s\n", deviceName,
               nabto_client_error_get_message(ec));
        nabto_client_connection_free(conn);
        nabto_client_free(client);
        nabtoshell_config_deinit(&config);
        return 1;
    }

    /* Get terminal size */
    uint16_t cols, rows;
    nabtoshell_terminal_get_size(&cols, &rows);

    /* Create new session if requested */
    if (createNew) {
        /* Generate a session name if not specified */
        char generatedName[64] = {0};
        if (sessionName == NULL) {
            snprintf(generatedName, sizeof(generatedName), "ns-%d", (int)getpid());
            sessionName = generatedName;
        }

        if (!nabtoshell_coap_create_session(conn, client, sessionName,
                                            cols, rows, newCommand)) {
            printf("Failed to create session '%s'\n", sessionName);
            nabto_client_connection_free(conn);
            nabto_client_free(client);
            nabtoshell_config_deinit(&config);
            return 1;
        }
    }

    /* Default session name */
    if (sessionName == NULL) {
        sessionName = "main";
    }

    /* Attach to session */
    if (!nabtoshell_coap_attach(conn, client, sessionName, cols, rows,
                                !createNew)) {
        printf("Failed to attach to session '%s'\n", sessionName);
        nabto_client_connection_free(conn);
        nabto_client_free(client);
        nabtoshell_config_deinit(&config);
        return 1;
    }

    /* Open stream on port 1 */
    NabtoClientStream* stream = nabto_client_stream_new(conn);
    if (stream == NULL) {
        printf("Failed to create stream\n");
        nabto_client_connection_free(conn);
        nabto_client_free(client);
        nabtoshell_config_deinit(&config);
        return 1;
    }

    future = nabto_client_future_new(client);
    nabto_client_stream_open(stream, future, 1);
    ec = nabto_client_future_wait(future);
    nabto_client_future_free(future);

    if (ec != NABTO_CLIENT_EC_OK) {
        printf("Failed to open stream: %s\n", nabto_client_error_get_message(ec));
        nabto_client_stream_free(stream);
        nabto_client_connection_free(conn);
        nabto_client_free(client);
        nabtoshell_config_deinit(&config);
        return 1;
    }

    /* Set terminal to raw mode */
    struct termios savedTermios;
    if (!nabtoshell_terminal_set_raw(&savedTermios)) {
        printf("Failed to set terminal to raw mode\n");
        nabto_client_stream_free(stream);
        nabto_client_connection_free(conn);
        nabto_client_free(client);
        nabtoshell_config_deinit(&config);
        return 1;
    }

    /* Install SIGWINCH handler */
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigwinch_handler;
    sa.sa_flags = SA_RESTART;
    sigaction(SIGWINCH, &sa, NULL);

    /* Start reader thread (stream -> stdout) */
    atomic_bool done = false;
    struct reader_thread_ctx readerCtx = {
        .client = client,
        .stream = stream,
        .done = &done,
    };

    pthread_t readerThread;
    if (pthread_create(&readerThread, NULL, stream_reader_thread, &readerCtx) != 0) {
        nabtoshell_terminal_restore(&savedTermios);
        printf("Failed to create reader thread\n");
        nabto_client_stream_free(stream);
        nabto_client_connection_free(conn);
        nabto_client_free(client);
        nabtoshell_config_deinit(&config);
        return 1;
    }

    /* Main loop: read stdin, send to stream, handle SIGWINCH */
    uint8_t inputBuf[READ_BUFFER_SIZE];

    while (!atomic_load(&done)) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);

        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 100000; /* 100ms timeout */

        int ret = select(STDIN_FILENO + 1, &readfds, NULL, NULL, &tv);

        /* Check for SIGWINCH */
        if (sigwinch_received) {
            sigwinch_received = 0;
            uint16_t newCols, newRows;
            if (nabtoshell_terminal_get_size(&newCols, &newRows)) {
                nabtoshell_coap_resize(conn, client, newCols, newRows);
            }
        }

        if (ret > 0 && FD_ISSET(STDIN_FILENO, &readfds)) {
            ssize_t n = read(STDIN_FILENO, inputBuf, sizeof(inputBuf));
            if (n <= 0) {
                break;
            }

            future = nabto_client_future_new(client);
            nabto_client_stream_write(stream, future, inputBuf, (size_t)n);
            ec = nabto_client_future_wait(future);
            nabto_client_future_free(future);

            if (ec != NABTO_CLIENT_EC_OK) {
                break;
            }
        }
    }

    /* Cleanup */
    atomic_store(&done, true);

    /* Close stream */
    future = nabto_client_future_new(client);
    nabto_client_stream_close(stream, future);
    nabto_client_future_wait(future);
    nabto_client_future_free(future);

    pthread_join(readerThread, NULL);

    /* Restore terminal */
    nabtoshell_terminal_restore(&savedTermios);

    nabto_client_stream_free(stream);

    /* Close connection */
    future = nabto_client_future_new(client);
    nabto_client_connection_close(conn, future);
    nabto_client_future_wait(future);
    nabto_client_future_free(future);

    nabto_client_connection_free(conn);
    nabto_client_free(client);
    nabtoshell_config_deinit(&config);

    return 0;
}
