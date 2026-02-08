#include "nabtoshell_coap_handler.h"
#include "nabtoshell.h"
#include "nabtoshell_stream.h"

#include <tinycbor/cbor.h>

#include <sys/ioctl.h>
#include <signal.h>

static void handle_request(struct nabtoshell_coap_handler* handler,
                           NabtoDeviceCoapRequest* request);

NabtoDeviceError nabtoshell_coap_resize_init(
    struct nabtoshell_coap_handler* handler, NabtoDevice* device,
    struct nabtoshell* app)
{
    const char* paths[] = {"terminal", "resize", NULL};
    return nabtoshell_coap_handler_init(handler, device, app,
                                        NABTO_DEVICE_COAP_POST, paths,
                                        &handle_request);
}

static void handle_request(struct nabtoshell_coap_handler* handler,
                           NabtoDeviceCoapRequest* request)
{
    struct nabtoshell* app = handler->app;

    if (!nabtoshell_iam_check_access(&app->iam, request, "Terminal:Resize")) {
        nabto_device_coap_error_response(request, 403, "Access denied");
        return;
    }

    CborParser parser;
    CborValue value;
    if (!nabtoshell_init_cbor_parser(request, &parser, &value)) {
        return;
    }

    if (!cbor_value_is_map(&value)) {
        nabto_device_coap_error_response(request, 400, "Expected map");
        return;
    }

    CborValue map;
    cbor_value_enter_container(&value, &map);

    uint64_t cols = 0, rows = 0;
    bool hasCols = false, hasRows = false;

    while (!cbor_value_at_end(&map)) {
        if (!cbor_value_is_text_string(&map)) {
            cbor_value_advance(&map);
            cbor_value_advance(&map);
            continue;
        }
        char keyBuf[32];
        size_t keyLen = sizeof(keyBuf);
        cbor_value_copy_text_string(&map, keyBuf, &keyLen, NULL);
        cbor_value_advance(&map);

        if (strcmp(keyBuf, "cols") == 0 && cbor_value_is_unsigned_integer(&map)) {
            cbor_value_get_uint64(&map, &cols);
            hasCols = true;
        } else if (strcmp(keyBuf, "rows") == 0 && cbor_value_is_unsigned_integer(&map)) {
            cbor_value_get_uint64(&map, &rows);
            hasRows = true;
        }
        cbor_value_advance(&map);
    }

    if (!hasCols || !hasRows) {
        nabto_device_coap_error_response(request, 400, "Missing cols or rows");
        return;
    }

    /* Find the PTY fd for this connection and resize it */
    NabtoDeviceConnectionRef ref = nabto_device_coap_request_get_connection_ref(request);
    int ptyFd = nabtoshell_stream_get_pty_fd(&app->streamListener, ref);
    if (ptyFd >= 0) {
        struct winsize ws;
        ws.ws_col = (unsigned short)cols;
        ws.ws_row = (unsigned short)rows;
        ws.ws_xpixel = 0;
        ws.ws_ypixel = 0;
        ioctl(ptyFd, TIOCSWINSZ, &ws);

        /* Also update session map dimensions */
        struct nabtoshell_session_entry* entry =
            nabtoshell_session_find(&app->sessionMap, ref);
        if (entry != NULL) {
            entry->cols = (uint16_t)cols;
            entry->rows = (uint16_t)rows;
        }
    }

    nabto_device_coap_response_set_code(request, 204);
    nabto_device_coap_response_ready(request);
}
