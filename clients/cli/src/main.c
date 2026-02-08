#include "nabtoshell_client.h"
#include "nabtoshell_pair.h"
#include "nabtoshell_attach.h"
#include "nabtoshell_sessions.h"
#include "nabtoshell_devices.h"
#include "nabtoshell_rename.h"

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
    } else if (strcmp(command, "attach") == 0 || strcmp(command, "a") == 0) {
        return nabtoshell_cmd_attach(argc - 1, argv + 1);
    } else if (strcmp(command, "create") == 0 ||
               strcmp(command, "new") == 0 ||
               strcmp(command, "new-session") == 0 ||
               strcmp(command, "c") == 0 ||
               strcmp(command, "n") == 0) {
        return nabtoshell_cmd_new_session(argc - 1, argv + 1);
    } else if (strcmp(command, "sessions") == 0) {
        return nabtoshell_cmd_sessions(argc - 1, argv + 1);
    } else if (strcmp(command, "devices") == 0) {
        return nabtoshell_cmd_devices(argc - 1, argv + 1);
    } else if (strcmp(command, "rename") == 0) {
        return nabtoshell_cmd_rename(argc - 1, argv + 1);
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
    printf("  attach <device> [session]      Attach to an existing tmux session (alias: a)\n");
    printf("  create <device> [session]      Create a new session and attach (aliases: new, n, c)\n");
    printf("  sessions <device>              List tmux sessions on a device\n");
    printf("  devices                        List saved device bookmarks\n");
    printf("  rename <current> <new-name>    Rename a device bookmark\n");
    printf("\n");
    printf("Options:\n");
    printf("  -h, --help                     Show this help\n");
    printf("  -v, --version                  Show version\n");
}
