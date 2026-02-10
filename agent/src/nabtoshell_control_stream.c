#include "nabtoshell_control_stream.h"
#include "nabtoshell.h"
#include "nabtoshell_tmux.h"

#include <tinycbor/cbor.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define NEWLINE "\n"

static void start_listen(struct nabtoshell_control_stream_listener* csl);
static void stream_callback(NabtoDeviceFuture* future, NabtoDeviceError ec,
                            void* userData);
static void* monitor_thread_func(void* arg);
static void ensure_monitor_started(struct nabtoshell_control_stream_listener* csl);
static uint8_t* encode_session_snapshot(const struct nabtoshell_tmux_list* list,
                                        size_t* outLen);
static bool tmux_lists_equal(const struct nabtoshell_tmux_list* a,
                             const struct nabtoshell_tmux_list* b);
static void broadcast_to_all(struct nabtoshell_control_stream_listener* csl,
                             const uint8_t* data, size_t len);

void nabtoshell_control_stream_listener_init(
    struct nabtoshell_control_stream_listener* csl,
    NabtoDevice* device,
    struct nabtoshell* app)
{
    memset(csl, 0, sizeof(*csl));
    csl->device = device;
    csl->app = app;
    atomic_init(&csl->monitorStop, false);

    pthread_mutex_init(&csl->streamListMutex, NULL);
    pthread_mutex_init(&csl->notifyMutex, NULL);
    pthread_cond_init(&csl->notifyCond, NULL);

    csl->listener = nabto_device_listener_new(device);
    csl->future = nabto_device_future_new(device);

    NabtoDeviceError ec = nabto_device_stream_init_listener(
        device, csl->listener, NABTOSHELL_CONTROL_STREAM_PORT);
    if (ec != NABTO_DEVICE_EC_OK) {
        printf("Failed to init control stream listener: %s" NEWLINE,
               nabto_device_error_get_message(ec));
        return;
    }

    start_listen(csl);
}

void nabtoshell_control_stream_listener_stop(
    struct nabtoshell_control_stream_listener* csl)
{
    if (csl->listener != NULL) {
        nabto_device_listener_stop(csl->listener);
    }

    /* Signal monitor thread to stop */
    atomic_store(&csl->monitorStop, true);
    pthread_mutex_lock(&csl->notifyMutex);
    pthread_cond_signal(&csl->notifyCond);
    pthread_mutex_unlock(&csl->notifyMutex);

    /* Mark all active control streams for closure */
    pthread_mutex_lock(&csl->streamListMutex);
    struct nabtoshell_active_control_stream* cs = csl->activeStreams;
    while (cs != NULL) {
        atomic_store(&cs->closing, true);
        cs = cs->next;
    }
    pthread_mutex_unlock(&csl->streamListMutex);
}

void nabtoshell_control_stream_listener_deinit(
    struct nabtoshell_control_stream_listener* csl)
{
    /* Wait for monitor thread */
    if (csl->monitorStarted) {
        pthread_join(csl->monitorThread, NULL);
        csl->monitorStarted = false;
    }

    /* Free all active control streams */
    pthread_mutex_lock(&csl->streamListMutex);
    struct nabtoshell_active_control_stream* cs = csl->activeStreams;
    csl->activeStreams = NULL;
    pthread_mutex_unlock(&csl->streamListMutex);

    while (cs != NULL) {
        struct nabtoshell_active_control_stream* next = cs->next;
        if (cs->stream != NULL) {
            nabto_device_stream_free(cs->stream);
        }
        pthread_mutex_destroy(&cs->writeMutex);
        free(cs);
        cs = next;
    }

    if (csl->future != NULL) {
        nabto_device_future_free(csl->future);
        csl->future = NULL;
    }
    if (csl->listener != NULL) {
        nabto_device_listener_free(csl->listener);
        csl->listener = NULL;
    }

    pthread_cond_destroy(&csl->notifyCond);
    pthread_mutex_destroy(&csl->notifyMutex);
    pthread_mutex_destroy(&csl->streamListMutex);
}

void nabtoshell_control_stream_notify(
    struct nabtoshell_control_stream_listener* csl)
{
    /* Non-blocking: just signal the condition variable to wake the monitor. */
    pthread_mutex_lock(&csl->notifyMutex);
    pthread_cond_signal(&csl->notifyCond);
    pthread_mutex_unlock(&csl->notifyMutex);
}

/* ---- Internal ---- */

static void start_listen(struct nabtoshell_control_stream_listener* csl)
{
    nabto_device_listener_new_stream(csl->listener, csl->future, &csl->newStream);
    nabto_device_future_set_callback(csl->future, stream_callback, csl);
}

/*
 * Called on the Nabto SDK event loop thread. MUST NOT block.
 * Accepts the control stream, does IAM check, adds to linked list,
 * then ensures the monitor thread is running.
 */
static void stream_accepted(NabtoDeviceFuture* future, NabtoDeviceError ec,
                            void* userData);

static void stream_callback(NabtoDeviceFuture* future, NabtoDeviceError ec,
                            void* userData)
{
    (void)future;
    struct nabtoshell_control_stream_listener* csl = userData;
    if (ec != NABTO_DEVICE_EC_OK) {
        return;
    }

    struct nabtoshell_active_control_stream* cs =
        calloc(1, sizeof(struct nabtoshell_active_control_stream));
    if (cs == NULL) {
        nabto_device_stream_free(csl->newStream);
        start_listen(csl);
        return;
    }

    cs->stream = csl->newStream;
    cs->device = csl->device;
    cs->app = csl->app;
    atomic_init(&cs->closing, false);
    pthread_mutex_init(&cs->writeMutex, NULL);

    /* Accept the stream */
    NabtoDeviceFuture* acceptFuture = nabto_device_future_new(csl->device);
    if (acceptFuture == NULL) {
        nabto_device_stream_free(cs->stream);
        pthread_mutex_destroy(&cs->writeMutex);
        free(cs);
        start_listen(csl);
        return;
    }
    nabto_device_stream_accept(cs->stream, acceptFuture);
    nabto_device_future_set_callback(acceptFuture, stream_accepted, cs);

    csl->newStream = NULL;
    start_listen(csl);
}

static void stream_accepted(NabtoDeviceFuture* future, NabtoDeviceError ec,
                            void* userData)
{
    struct nabtoshell_active_control_stream* cs = userData;
    nabto_device_future_free(future);

    if (ec != NABTO_DEVICE_EC_OK) {
        nabto_device_stream_free(cs->stream);
        pthread_mutex_destroy(&cs->writeMutex);
        free(cs);
        return;
    }

    /* IAM check */
    NabtoDeviceConnectionRef ref =
        nabto_device_stream_get_connection_ref(cs->stream);
    if (!nabtoshell_iam_check_access_ref(&cs->app->iam, ref, "Terminal:ListSessions")) {
        nabto_device_stream_free(cs->stream);
        pthread_mutex_destroy(&cs->writeMutex);
        free(cs);
        return;
    }

    /* Add to linked list (under mutex, quick operation) */
    struct nabtoshell_control_stream_listener* csl =
        &cs->app->controlStreamListener;
    pthread_mutex_lock(&csl->streamListMutex);
    cs->next = csl->activeStreams;
    csl->activeStreams = cs;
    pthread_mutex_unlock(&csl->streamListMutex);

    /* Ensure monitor thread is running and wake it for initial snapshot */
    ensure_monitor_started(csl);
    nabtoshell_control_stream_notify(csl);
}

static void ensure_monitor_started(struct nabtoshell_control_stream_listener* csl)
{
    pthread_mutex_lock(&csl->streamListMutex);
    if (!csl->monitorStarted) {
        if (pthread_create(&csl->monitorThread, NULL, monitor_thread_func, csl) == 0) {
            csl->monitorStarted = true;
        } else {
            printf("Failed to start control stream monitor thread" NEWLINE);
        }
    }
    pthread_mutex_unlock(&csl->streamListMutex);
}

/*
 * Monitor thread: polls tmux session list periodically and on notify,
 * broadcasts changes to all connected control streams. This thread owns
 * all blocking I/O (tmux fork, stream writes), so it is safe to block here.
 */
static void* monitor_thread_func(void* arg)
{
    struct nabtoshell_control_stream_listener* csl = arg;
    struct nabtoshell_tmux_list prevList;
    memset(&prevList, 0, sizeof(prevList));
    bool hasPrev = false;

    while (!atomic_load(&csl->monitorStop)) {
        /* Poll tmux sessions */
        struct nabtoshell_tmux_list currentList;
        memset(&currentList, 0, sizeof(currentList));
        nabtoshell_tmux_list_sessions(&currentList);

        bool changed = !hasPrev || !tmux_lists_equal(&prevList, &currentList);

        if (changed) {
            size_t msgLen = 0;
            uint8_t* msg = encode_session_snapshot(&currentList, &msgLen);
            if (msg != NULL) {
                broadcast_to_all(csl, msg, msgLen);
                free(msg);
            }
            prevList = currentList;
            hasPrev = true;
        }

        /* Remove closed streams from the list */
        pthread_mutex_lock(&csl->streamListMutex);
        struct nabtoshell_active_control_stream** pp = &csl->activeStreams;
        while (*pp != NULL) {
            if (atomic_load(&(*pp)->closing)) {
                struct nabtoshell_active_control_stream* dead = *pp;
                *pp = dead->next;
                if (dead->stream != NULL) {
                    nabto_device_stream_free(dead->stream);
                }
                pthread_mutex_destroy(&dead->writeMutex);
                free(dead);
            } else {
                pp = &(*pp)->next;
            }
        }
        bool hasStreams = (csl->activeStreams != NULL);
        pthread_mutex_unlock(&csl->streamListMutex);

        if (!hasStreams && hasPrev) {
            /* No more control streams connected. Reset state so
             * we send a full snapshot on the next connection. */
            hasPrev = false;
        }

        /* Wait for notify or timeout */
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        long ms = NABTOSHELL_SESSION_POLL_INTERVAL_MS;
        ts.tv_sec += ms / 1000;
        ts.tv_nsec += (ms % 1000) * 1000000L;
        if (ts.tv_nsec >= 1000000000L) {
            ts.tv_sec += 1;
            ts.tv_nsec -= 1000000000L;
        }

        pthread_mutex_lock(&csl->notifyMutex);
        pthread_cond_timedwait(&csl->notifyCond, &csl->notifyMutex, &ts);
        pthread_mutex_unlock(&csl->notifyMutex);
    }

    return NULL;
}

static bool tmux_lists_equal(const struct nabtoshell_tmux_list* a,
                             const struct nabtoshell_tmux_list* b)
{
    if (a->count != b->count) {
        return false;
    }
    for (int i = 0; i < a->count; i++) {
        if (strcmp(a->sessions[i].name, b->sessions[i].name) != 0 ||
            a->sessions[i].cols != b->sessions[i].cols ||
            a->sessions[i].rows != b->sessions[i].rows ||
            a->sessions[i].attached != b->sessions[i].attached) {
            return false;
        }
    }
    return true;
}

/*
 * Encode a session list as a length-prefixed CBOR message:
 *   [4 bytes: big-endian uint32 payload length][N bytes: CBOR]
 *
 * CBOR format: {"sessions": [{name, cols, rows, attached}, ...]}
 */
static uint8_t* encode_session_snapshot(const struct nabtoshell_tmux_list* list,
                                        size_t* outLen)
{
    uint8_t cborBuf[2048];
    CborEncoder encoder;
    cbor_encoder_init(&encoder, cborBuf, sizeof(cborBuf), 0);

    CborEncoder mapEncoder;
    cbor_encoder_create_map(&encoder, &mapEncoder, 1);

    cbor_encode_text_stringz(&mapEncoder, "sessions");

    CborEncoder arrayEncoder;
    cbor_encoder_create_array(&mapEncoder, &arrayEncoder, list->count);

    for (int i = 0; i < list->count; i++) {
        CborEncoder sessionMap;
        cbor_encoder_create_map(&arrayEncoder, &sessionMap, 4);

        cbor_encode_text_stringz(&sessionMap, "name");
        cbor_encode_text_stringz(&sessionMap, list->sessions[i].name);

        cbor_encode_text_stringz(&sessionMap, "cols");
        cbor_encode_uint(&sessionMap, list->sessions[i].cols);

        cbor_encode_text_stringz(&sessionMap, "rows");
        cbor_encode_uint(&sessionMap, list->sessions[i].rows);

        cbor_encode_text_stringz(&sessionMap, "attached");
        cbor_encode_uint(&sessionMap, list->sessions[i].attached);

        cbor_encoder_close_container(&arrayEncoder, &sessionMap);
    }

    cbor_encoder_close_container(&mapEncoder, &arrayEncoder);
    cbor_encoder_close_container(&encoder, &mapEncoder);

    size_t cborLen = cbor_encoder_get_buffer_size(&encoder, cborBuf);

    /* Allocate: 4-byte length prefix + CBOR payload */
    size_t totalLen = 4 + cborLen;
    uint8_t* buf = malloc(totalLen);
    if (buf == NULL) {
        return NULL;
    }

    /* Big-endian uint32 length prefix */
    uint32_t lenBE = htonl((uint32_t)cborLen);
    memcpy(buf, &lenBE, 4);
    memcpy(buf + 4, cborBuf, cborLen);

    *outLen = totalLen;
    return buf;
}

/*
 * Send the encoded message to all connected control streams.
 * Called from the monitor thread, so blocking writes are safe.
 *
 * Takes a snapshot of active stream pointers under the mutex, then
 * iterates the snapshot without holding the list lock. Each stream's
 * writeMutex serializes the actual I/O and the closing flag guards
 * against writes to freed streams.
 */
#define MAX_CONTROL_STREAMS 16
static void broadcast_to_all(struct nabtoshell_control_stream_listener* csl,
                             const uint8_t* data, size_t len)
{
    struct nabtoshell_active_control_stream* snapshot[MAX_CONTROL_STREAMS];
    int count = 0;

    pthread_mutex_lock(&csl->streamListMutex);
    struct nabtoshell_active_control_stream* cs = csl->activeStreams;
    while (cs != NULL && count < MAX_CONTROL_STREAMS) {
        if (!atomic_load(&cs->closing)) {
            snapshot[count++] = cs;
        }
        cs = cs->next;
    }
    pthread_mutex_unlock(&csl->streamListMutex);

    for (int i = 0; i < count; i++) {
        cs = snapshot[i];
        if (atomic_load(&cs->closing)) {
            continue;
        }
        pthread_mutex_lock(&cs->writeMutex);
        if (!atomic_load(&cs->closing)) {
            NabtoDeviceFuture* writeFuture = nabto_device_future_new(cs->device);
            if (writeFuture != NULL) {
                nabto_device_stream_write(cs->stream, writeFuture, data, len);
                NabtoDeviceError ec = nabto_device_future_wait(writeFuture);
                nabto_device_future_free(writeFuture);
                if (ec != NABTO_DEVICE_EC_OK) {
                    atomic_store(&cs->closing, true);
                }
            }
        }
        pthread_mutex_unlock(&cs->writeMutex);
    }
}
