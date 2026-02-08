#include "nabtoshell_client.h"
#include "nabtoshell_pair.h"
#include "nabtoshell_connect.h"
#include "nabtoshell_sessions.h"
#include "nabtoshell_devices.h"

#include <stdio.h>
#include <string.h>

static void print_help(void);

int main(int argc, char** argv)
{
    if (argc < 2) {
        print_help();
        return 1;
    }

    const char* command = argv[1];

    if (strcmp(command, "pair") == 0) {
        return nabtoshell_cmd_pair(argc - 1, argv + 1);
    } else if (strcmp(command, "connect") == 0) {
        return nabtoshell_cmd_connect(argc - 1, argv + 1);
    } else if (strcmp(command, "sessions") == 0) {
        return nabtoshell_cmd_sessions(argc - 1, argv + 1);
    } else if (strcmp(command, "devices") == 0) {
        return nabtoshell_cmd_devices(argc - 1, argv + 1);
    } else if (strcmp(command, "--help") == 0 || strcmp(command, "-h") == 0) {
        print_help();
        return 0;
    } else if (strcmp(command, "--version") == 0 || strcmp(command, "-v") == 0) {
        printf("%s\n", NABTOSHELL_CLIENT_VERSION);
        return 0;
    } else {
        printf("Unknown command: %s\n", command);
        print_help();
        return 1;
    }
}

void print_help(void)
{
    printf("NabtoShell CLI Client v%s\n", NABTOSHELL_CLIENT_VERSION);
    printf("\n");
    printf("Usage: nabtoshell <command> [options]\n");
    printf("\n");
    printf("Commands:\n");
    printf("  pair <pairing-string>          One-time pairing with a device\n");
    printf("  connect <device-name>          Connect to a device\n");
    printf("  sessions <device-name>         List tmux sessions on a device\n");
    printf("  devices                        List saved device bookmarks\n");
    printf("\n");
    printf("Options:\n");
    printf("  -h, --help                     Show this help\n");
    printf("  -v, --version                  Show version\n");
}
