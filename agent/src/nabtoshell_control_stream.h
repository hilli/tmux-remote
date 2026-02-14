#ifndef NABTOSHELL_CONTROL_STREAM_H_
#define NABTOSHELL_CONTROL_STREAM_H_

#include <nabto/nabto_device.h>

#include <pthread.h>
#include <stdbool.h>
#include <stdatomic.h>

#include "nabtoshell_prompt.h"

#define NABTOSHELL_CONTROL_STREAM_PORT 2
#define NABTOSHELL_SESSION_POLL_INTERVAL_MS 2000
#define MAX_CONTROL_STREAMS 16

struct nabtoshell;

struct nabtoshell_active_control_stream {
    NabtoDeviceStream* stream;
    NabtoDevice* device;
    struct nabtoshell* app;
    pthread_mutex_t writeMutex;
    atomic_bool closing;
    atomic_bool needsPromptSync;
    atomic_bool needsSessionSync;
    NabtoDeviceConnectionRef connectionRef;
    atomic_uint refCount;
    pthread_t readerThread;
    bool readerThreadStarted;
    struct nabtoshell_active_control_stream* next;
};

struct nabtoshell_control_stream_listener {
    NabtoDevice* device;
    struct nabtoshell* app;
    NabtoDeviceListener* listener;
    NabtoDeviceFuture* future;
    NabtoDeviceStream* newStream;

    pthread_t monitorThread;
    bool monitorStarted;
    atomic_bool monitorStop;

    pthread_mutex_t streamListMutex;
    struct nabtoshell_active_control_stream* activeStreams;

    pthread_mutex_t notifyMutex;
    pthread_cond_t notifyCond;
};

void nabtoshell_control_stream_listener_init(
    struct nabtoshell_control_stream_listener* csl,
    NabtoDevice* device,
    struct nabtoshell* app);

void nabtoshell_control_stream_listener_stop(
    struct nabtoshell_control_stream_listener* csl);

void nabtoshell_control_stream_listener_join_monitor(
    struct nabtoshell_control_stream_listener* csl);

void nabtoshell_control_stream_listener_deinit(
    struct nabtoshell_control_stream_listener* csl);

void nabtoshell_control_stream_notify(
    struct nabtoshell_control_stream_listener* csl);

void nabtoshell_control_stream_send_prompt_present_for_ref(
    struct nabtoshell_control_stream_listener* csl,
    NabtoDeviceConnectionRef ref,
    const nabtoshell_prompt_instance* instance);

void nabtoshell_control_stream_send_prompt_update_for_ref(
    struct nabtoshell_control_stream_listener* csl,
    NabtoDeviceConnectionRef ref,
    const nabtoshell_prompt_instance* instance);

void nabtoshell_control_stream_send_prompt_gone_for_ref(
    struct nabtoshell_control_stream_listener* csl,
    NabtoDeviceConnectionRef ref,
    const char* instance_id);

#ifdef NABTOSHELL_TESTING
int nabtoshell_control_stream_collect_targets_for_ref(
    struct nabtoshell_control_stream_listener* csl,
    NabtoDeviceConnectionRef ref,
    struct nabtoshell_active_control_stream** out,
    int cap);

void nabtoshell_control_stream_release(struct nabtoshell_active_control_stream* cs);
#endif

#endif
