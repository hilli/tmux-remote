#ifndef NABTOSHELL_STREAM_H_
#define NABTOSHELL_STREAM_H_

#include <nabto/nabto_device.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <pthread.h>
#include <sys/types.h>

#define NABTOSHELL_STREAM_BUFFER_SIZE 4096
#define NABTOSHELL_MAX_ACTIVE_STREAMS 8

struct nabtoshell;

struct nabtoshell_active_stream {
    NabtoDevice* device;
    NabtoDeviceStream* stream;
    struct nabtoshell* app;
    int ptyFd;
    pid_t childPid;
    atomic_bool closing;
    pthread_t ptyReaderThread;
    bool threadStarted;

    uint8_t readBuffer[NABTOSHELL_STREAM_BUFFER_SIZE];
    size_t readLength;

    struct nabtoshell_active_stream* next;
};

struct nabtoshell_stream_listener {
    NabtoDevice* device;
    struct nabtoshell* app;
    NabtoDeviceListener* listener;
    NabtoDeviceFuture* future;
    NabtoDeviceStream* newStream;

    struct nabtoshell_active_stream* activeStreams;
};

void nabtoshell_stream_listener_init(struct nabtoshell_stream_listener* sl,
                                     NabtoDevice* device,
                                     struct nabtoshell* app);
void nabtoshell_stream_listener_stop(struct nabtoshell_stream_listener* sl);
void nabtoshell_stream_listener_deinit(struct nabtoshell_stream_listener* sl);

int nabtoshell_stream_get_pty_fd(struct nabtoshell_stream_listener* sl,
                                 NabtoDeviceConnectionRef ref);

#endif
