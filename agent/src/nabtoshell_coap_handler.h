#ifndef NABTOSHELL_COAP_HANDLER_H_
#define NABTOSHELL_COAP_HANDLER_H_

#include <nabto/nabto_device.h>
#include <tinycbor/cbor.h>

struct nabtoshell;
struct nabtoshell_coap_handler;

typedef void (*nabtoshell_coap_request_handler)(
    struct nabtoshell_coap_handler* handler,
    NabtoDeviceCoapRequest* request);

struct nabtoshell_coap_handler {
    NabtoDevice* device;
    struct nabtoshell* app;
    NabtoDeviceFuture* future;
    NabtoDeviceListener* listener;
    NabtoDeviceCoapRequest* request;
    nabtoshell_coap_request_handler requestHandler;
};

NabtoDeviceError nabtoshell_coap_handler_init(
    struct nabtoshell_coap_handler* handler,
    NabtoDevice* device,
    struct nabtoshell* app,
    NabtoDeviceCoapMethod method,
    const char** paths,
    nabtoshell_coap_request_handler requestHandler);

void nabtoshell_coap_handler_stop(struct nabtoshell_coap_handler* handler);
void nabtoshell_coap_handler_deinit(struct nabtoshell_coap_handler* handler);

bool nabtoshell_init_cbor_parser(NabtoDeviceCoapRequest* request,
                                 CborParser* parser, CborValue* cborValue);

/* Per-endpoint init functions */
NabtoDeviceError nabtoshell_coap_resize_init(
    struct nabtoshell_coap_handler* handler, NabtoDevice* device,
    struct nabtoshell* app);

NabtoDeviceError nabtoshell_coap_sessions_init(
    struct nabtoshell_coap_handler* handler, NabtoDevice* device,
    struct nabtoshell* app);

NabtoDeviceError nabtoshell_coap_attach_init(
    struct nabtoshell_coap_handler* handler, NabtoDevice* device,
    struct nabtoshell* app);

NabtoDeviceError nabtoshell_coap_create_init(
    struct nabtoshell_coap_handler* handler, NabtoDevice* device,
    struct nabtoshell* app);

NabtoDeviceError nabtoshell_coap_status_init(
    struct nabtoshell_coap_handler* handler, NabtoDevice* device,
    struct nabtoshell* app);

#endif
