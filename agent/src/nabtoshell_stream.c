#include "nabtoshell_stream.h"
#include "nabtoshell.h"
#include "nabtoshell_session.h"
#include "nabtoshell_tmux.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <errno.h>

#ifdef __APPLE__
#include <util.h>
#else
#include <pty.h>
#endif

#define NEWLINE "\n"

static void start_listen(struct nabtoshell_stream_listener* sl);
static void stream_callback(NabtoDeviceFuture* future, NabtoDeviceError ec,
                            void* userData);
static void stream_accepted(NabtoDeviceFuture* future, NabtoDeviceError ec,
                            void* userData);
static void start_stream_read(struct nabtoshell_active_stream* as);
static void stream_has_read(NabtoDeviceFuture* future, NabtoDeviceError ec,
                            void* userData);
static void stream_wrote(NabtoDeviceFuture* future, NabtoDeviceError ec,
                         void* userData);
static void start_stream_close(struct nabtoshell_active_stream* as);
static void stream_closed(NabtoDeviceFuture* future, NabtoDeviceError ec,
                          void* userData);
static void cleanup_active_stream(struct nabtoshell_active_stream* as);
static void* pty_reader_thread(void* arg);

void nabtoshell_stream_listener_init(struct nabtoshell_stream_listener* sl,
                                     NabtoDevice* device,
                                     struct nabtoshell* app)
{
    memset(sl, 0, sizeof(struct nabtoshell_stream_listener));
    sl->device = device;
    sl->app = app;

    sl->listener = nabto_device_listener_new(device);
    sl->future = nabto_device_future_new(device);

    NabtoDeviceError ec = nabto_device_stream_init_listener(device, sl->listener, 1);
    if (ec != NABTO_DEVICE_EC_OK) {
        printf("Failed to init stream listener: %s" NEWLINE,
               nabto_device_error_get_message(ec));
        return;
    }

    start_listen(sl);
}

void nabtoshell_stream_listener_stop(struct nabtoshell_stream_listener* sl)
{
    if (sl->listener != NULL) {
        nabto_device_listener_stop(sl->listener);
    }

    /* Close all active streams */
    struct nabtoshell_active_stream* as = sl->activeStreams;
    while (as != NULL) {
        atomic_store(&as->closing, true);
        as = as->next;
    }
}

void nabtoshell_stream_listener_deinit(struct nabtoshell_stream_listener* sl)
{
    /* Wait for and clean up active streams */
    struct nabtoshell_active_stream* as = sl->activeStreams;
    while (as != NULL) {
        struct nabtoshell_active_stream* next = as->next;
        atomic_store(&as->closing, true);
        if (as->ptyFd >= 0) {
            close(as->ptyFd);
            as->ptyFd = -1;
        }
        if (as->threadStarted) {
            pthread_join(as->ptyReaderThread, NULL);
        }
        if (as->childPid > 0) {
            waitpid(as->childPid, NULL, WNOHANG);
        }
        if (as->stream != NULL) {
            nabto_device_stream_free(as->stream);
        }
        free(as);
        as = next;
    }
    sl->activeStreams = NULL;

    if (sl->future != NULL) {
        nabto_device_future_free(sl->future);
        sl->future = NULL;
    }
    if (sl->listener != NULL) {
        nabto_device_listener_free(sl->listener);
        sl->listener = NULL;
    }
}

int nabtoshell_stream_get_pty_fd(struct nabtoshell_stream_listener* sl,
                                 NabtoDeviceConnectionRef ref)
{
    struct nabtoshell_active_stream* as = sl->activeStreams;
    while (as != NULL) {
        if (as->stream != NULL && !atomic_load(&as->closing)) {
            NabtoDeviceConnectionRef streamRef =
                nabto_device_stream_get_connection_ref(as->stream);
            if (streamRef == ref && as->ptyFd >= 0) {
                return as->ptyFd;
            }
        }
        as = as->next;
    }
    return -1;
}

static void start_listen(struct nabtoshell_stream_listener* sl)
{
    nabto_device_listener_new_stream(sl->listener, sl->future, &sl->newStream);
    nabto_device_future_set_callback(sl->future, stream_callback, sl);
}

static void stream_callback(NabtoDeviceFuture* future, NabtoDeviceError ec,
                            void* userData)
{
    (void)future;
    struct nabtoshell_stream_listener* sl = userData;
    if (ec != NABTO_DEVICE_EC_OK) {
        return;
    }

    /* Create active stream state */
    struct nabtoshell_active_stream* as =
        (struct nabtoshell_active_stream*)calloc(1, sizeof(struct nabtoshell_active_stream));
    if (as == NULL) {
        nabto_device_stream_free(sl->newStream);
        start_listen(sl);
        return;
    }

    as->device = sl->device;
    as->stream = sl->newStream;
    as->app = sl->app;
    as->ptyFd = -1;
    as->childPid = -1;
    atomic_init(&as->closing, false);

    /* Add to linked list */
    as->next = sl->activeStreams;
    sl->activeStreams = as;

    /* Accept the stream */
    NabtoDeviceFuture* acceptFuture = nabto_device_future_new(sl->device);
    nabto_device_stream_accept(as->stream, acceptFuture);
    nabto_device_future_set_callback(acceptFuture, stream_accepted, as);

    /* Ready for the next stream */
    sl->newStream = NULL;
    start_listen(sl);
}

static void stream_accepted(NabtoDeviceFuture* future, NabtoDeviceError ec,
                            void* userData)
{
    nabto_device_future_free(future);
    struct nabtoshell_active_stream* as = userData;

    if (ec != NABTO_DEVICE_EC_OK) {
        cleanup_active_stream(as);
        return;
    }

    /* IAM check */
    NabtoDeviceConnectionRef ref =
        nabto_device_stream_get_connection_ref(as->stream);
    if (!nabtoshell_iam_check_access_ref(&as->app->iam, ref, "Terminal:Connect")) {
        start_stream_close(as);
        return;
    }

    /* Look up session target */
    struct nabtoshell_session_entry* entry =
        nabtoshell_session_find(&as->app->sessionMap, ref);
    if (entry == NULL) {
        printf("No session target set for connection, closing stream" NEWLINE);
        start_stream_close(as);
        return;
    }

    /* Set up initial window size */
    struct winsize ws;
    ws.ws_col = entry->cols;
    ws.ws_row = entry->rows;
    ws.ws_xpixel = 0;
    ws.ws_ypixel = 0;

    /* forkpty to spawn tmux attach */
    pid_t pid = forkpty(&as->ptyFd, NULL, NULL, &ws);
    if (pid < 0) {
        printf("forkpty failed: %s" NEWLINE, strerror(errno));
        start_stream_close(as);
        return;
    }

    if (pid == 0) {
        /* Child process: exec tmux attach */
        execlp("tmux", "tmux", "attach-session", "-t", entry->sessionName,
               (char*)NULL);
        /* If exec fails, try new-session as fallback */
        execlp("tmux", "tmux", "new-session", "-s", entry->sessionName,
               (char*)NULL);
        _exit(1);
    }

    /* Parent process */
    as->childPid = pid;

    /* Start the PTY reader thread */
    if (pthread_create(&as->ptyReaderThread, NULL, pty_reader_thread, as) != 0) {
        printf("Failed to create PTY reader thread" NEWLINE);
        close(as->ptyFd);
        as->ptyFd = -1;
        kill(pid, SIGTERM);
        waitpid(pid, NULL, 0);
        start_stream_close(as);
        return;
    }
    as->threadStarted = true;

    /* Start reading from the Nabto stream (client -> PTY) */
    start_stream_read(as);
}

static void start_stream_read(struct nabtoshell_active_stream* as)
{
    if (atomic_load(&as->closing)) {
        return;
    }

    NabtoDeviceFuture* readFuture = nabto_device_future_new(as->device);
    nabto_device_stream_read_some(as->stream, readFuture, as->readBuffer,
                                  NABTOSHELL_STREAM_BUFFER_SIZE, &as->readLength);
    nabto_device_future_set_callback(readFuture, stream_has_read, as);
}

static void stream_has_read(NabtoDeviceFuture* future, NabtoDeviceError ec,
                            void* userData)
{
    nabto_device_future_free(future);
    struct nabtoshell_active_stream* as = userData;

    if (ec == NABTO_DEVICE_EC_EOF || ec != NABTO_DEVICE_EC_OK) {
        atomic_store(&as->closing, true);
        if (as->ptyFd >= 0) {
            close(as->ptyFd);
            as->ptyFd = -1;
        }
        start_stream_close(as);
        return;
    }

    /* Write data from stream to PTY */
    if (as->ptyFd >= 0 && as->readLength > 0) {
        ssize_t written = write(as->ptyFd, as->readBuffer, as->readLength);
        (void)written;
    }

    start_stream_read(as);
}

/* PTY reader thread: reads from PTY fd and writes to Nabto stream.
   Uses blocking I/O on both sides. */
static void* pty_reader_thread(void* arg)
{
    struct nabtoshell_active_stream* as = arg;
    uint8_t buf[NABTOSHELL_STREAM_BUFFER_SIZE];

    while (!atomic_load(&as->closing)) {
        ssize_t n = read(as->ptyFd, buf, sizeof(buf));
        if (n <= 0) {
            break;
        }

        /* Write to Nabto stream (blocking) */
        NabtoDeviceFuture* writeFuture = nabto_device_future_new(as->device);
        nabto_device_stream_write(as->stream, writeFuture, buf, (size_t)n);
        NabtoDeviceError ec = nabto_device_future_wait(writeFuture);
        nabto_device_future_free(writeFuture);

        if (ec != NABTO_DEVICE_EC_OK) {
            break;
        }
    }

    /* Signal that we are done */
    atomic_store(&as->closing, true);

    /* Close the stream from the PTY reader side */
    NabtoDeviceFuture* closeFuture = nabto_device_future_new(as->device);
    nabto_device_stream_close(as->stream, closeFuture);
    nabto_device_future_wait(closeFuture);
    nabto_device_future_free(closeFuture);

    return NULL;
}

static void start_stream_close(struct nabtoshell_active_stream* as)
{
    NabtoDeviceFuture* closeFuture = nabto_device_future_new(as->device);
    nabto_device_stream_close(as->stream, closeFuture);
    nabto_device_future_set_callback(closeFuture, stream_closed, as);
}

static void stream_closed(NabtoDeviceFuture* future, NabtoDeviceError ec,
                          void* userData)
{
    (void)ec;
    nabto_device_future_free(future);
    struct nabtoshell_active_stream* as = userData;
    cleanup_active_stream(as);
}

static void cleanup_active_stream(struct nabtoshell_active_stream* as)
{
    atomic_store(&as->closing, true);

    if (as->ptyFd >= 0) {
        close(as->ptyFd);
        as->ptyFd = -1;
    }

    if (as->threadStarted) {
        pthread_join(as->ptyReaderThread, NULL);
        as->threadStarted = false;
    }

    if (as->childPid > 0) {
        int status;
        waitpid(as->childPid, &status, WNOHANG);
        as->childPid = -1;
    }

    /* Remove from session map */
    if (as->stream != NULL && as->app != NULL) {
        NabtoDeviceConnectionRef ref =
            nabto_device_stream_get_connection_ref(as->stream);
        nabtoshell_session_remove(&as->app->sessionMap, ref);
    }

    /* Remove from the linked list */
    if (as->app != NULL) {
        struct nabtoshell_stream_listener* sl = &as->app->streamListener;
        struct nabtoshell_active_stream** pp = &sl->activeStreams;
        while (*pp != NULL) {
            if (*pp == as) {
                *pp = as->next;
                break;
            }
            pp = &(*pp)->next;
        }
    }

    if (as->stream != NULL) {
        nabto_device_stream_free(as->stream);
        as->stream = NULL;
    }
    free(as);
}
