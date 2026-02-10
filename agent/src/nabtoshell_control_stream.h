#ifndef NABTOSHELL_CONTROL_STREAM_H_
#define NABTOSHELL_CONTROL_STREAM_H_

#include <nabto/nabto_device.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <pthread.h>

#define NABTOSHELL_CONTROL_STREAM_PORT 2
#define NABTOSHELL_SESSION_POLL_INTERVAL_MS 2000

struct nabtoshell;

struct nabtoshell_active_control_stream {
    NabtoDeviceStream* stream;
    NabtoDevice* device;
    struct nabtoshell* app;
    pthread_mutex_t writeMutex;
    atomic_bool closing;
    struct nabtoshell_active_control_stream* next;
};

struct nabtoshell_control_stream_listener {
    NabtoDevice* device;
    struct nabtoshell* app;
    NabtoDeviceListener* listener;
    NabtoDeviceFuture* future;
    NabtoDeviceStream* newStream;

    /* Session monitor thread */
    pthread_t monitorThread;
    bool monitorStarted;
    atomic_bool monitorStop;

    /* Protects activeStreams linked list */
    pthread_mutex_t streamListMutex;
    struct nabtoshell_active_control_stream* activeStreams;

    /* Condition variable for waking monitor (notify or periodic) */
    pthread_mutex_t notifyMutex;
    pthread_cond_t notifyCond;
};

void nabtoshell_control_stream_listener_init(
    struct nabtoshell_control_stream_listener* csl,
    NabtoDevice* device,
    struct nabtoshell* app);

void nabtoshell_control_stream_listener_stop(
    struct nabtoshell_control_stream_listener* csl);

void nabtoshell_control_stream_listener_deinit(
    struct nabtoshell_control_stream_listener* csl);

/* Wake the monitor thread to do an immediate poll+broadcast. Safe to call
 * from any thread including Nabto callback threads (non-blocking). */
void nabtoshell_control_stream_notify(
    struct nabtoshell_control_stream_listener* csl);

#endif
