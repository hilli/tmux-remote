#include "nabtoshell_tmux.h"

#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>

#define NEWLINE "\n"

bool nabtoshell_tmux_validate_session_name(const char* name)
{
    if (name == NULL || name[0] == '\0') {
        return false;
    }

    size_t len = strlen(name);
    if (len > 60) {
        return false;
    }

    for (size_t i = 0; i < len; i++) {
        char c = name[i];
        if (isalnum((unsigned char)c) || c == '.' || c == '_' || c == '-') {
            continue;
        }
        return false;
    }

    return true;
}

bool nabtoshell_tmux_list_sessions(struct nabtoshell_tmux_list* list)
{
    memset(list, 0, sizeof(struct nabtoshell_tmux_list));

    int pipefd[2];
    if (pipe(pipefd) < 0) {
        return false;
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        return false;
    }

    if (pid == 0) {
        /* Child: exec tmux list-sessions */
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);

        /* Redirect stderr to /dev/null so "no server running" is silent */
        int devnull = open("/dev/null", 0x0001); /* O_WRONLY */
        if (devnull >= 0) {
            dup2(devnull, STDERR_FILENO);
            close(devnull);
        }

        execlp("tmux", "tmux", "list-sessions", "-F",
               "#{session_name}:#{session_width}:#{session_height}:#{session_attached}",
               (char*)NULL);
        _exit(1);
    }

    /* Parent: read output */
    close(pipefd[1]);

    char buf[4096];
    ssize_t total = 0;
    ssize_t n;
    while ((n = read(pipefd[0], buf + total,
                     sizeof(buf) - (size_t)total - 1)) > 0) {
        total += n;
        if ((size_t)total >= sizeof(buf) - 1) {
            break;
        }
    }
    close(pipefd[0]);
    buf[total] = '\0';

    int status;
    waitpid(pid, &status, 0);

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        /* tmux not running or error: return empty list */
        return true;
    }

    /* Parse output: each line is name:width:height:attached */
    char* line = buf;
    while (line != NULL && *line != '\0' &&
           list->count < NABTOSHELL_TMUX_MAX_SESSIONS) {
        char* next = strchr(line, '\n');
        if (next != NULL) {
            *next = '\0';
            next++;
        }

        if (strlen(line) == 0) {
            line = next;
            continue;
        }

        char* nameEnd = strchr(line, ':');
        if (nameEnd == NULL) {
            line = next;
            continue;
        }
        *nameEnd = '\0';

        strncpy(list->sessions[list->count].name, line,
                sizeof(list->sessions[list->count].name) - 1);

        char* widthStr = nameEnd + 1;
        char* widthEnd = strchr(widthStr, ':');
        if (widthEnd == NULL) {
            line = next;
            continue;
        }
        *widthEnd = '\0';

        char* heightStr = widthEnd + 1;
        char* heightEnd = strchr(heightStr, ':');
        if (heightEnd == NULL) {
            line = next;
            continue;
        }
        *heightEnd = '\0';

        char* attachedStr = heightEnd + 1;

        list->sessions[list->count].cols = (uint16_t)atoi(widthStr);
        list->sessions[list->count].rows = (uint16_t)atoi(heightStr);
        list->sessions[list->count].attached = atoi(attachedStr);

        list->count++;
        line = next;
    }

    return true;
}

bool nabtoshell_tmux_session_exists(const char* name)
{
    if (!nabtoshell_tmux_validate_session_name(name)) {
        return false;
    }

    pid_t pid = fork();
    if (pid < 0) {
        return false;
    }

    if (pid == 0) {
        /* Redirect all output to /dev/null */
        int devnull = open("/dev/null", 0x0002); /* O_RDWR */
        if (devnull >= 0) {
            dup2(devnull, STDOUT_FILENO);
            dup2(devnull, STDERR_FILENO);
            close(devnull);
        }
        execlp("tmux", "tmux", "has-session", "-t", name, (char*)NULL);
        _exit(1);
    }

    int status;
    waitpid(pid, &status, 0);
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

bool nabtoshell_tmux_create_session(const char* name, uint16_t cols,
                                    uint16_t rows, const char* command)
{
    if (!nabtoshell_tmux_validate_session_name(name)) {
        return false;
    }

    char colsStr[16], rowsStr[16];
    snprintf(colsStr, sizeof(colsStr), "%u", cols);
    snprintf(rowsStr, sizeof(rowsStr), "%u", rows);

    pid_t pid = fork();
    if (pid < 0) {
        return false;
    }

    if (pid == 0) {
        /* Redirect output to /dev/null */
        int devnull = open("/dev/null", 0x0002);
        if (devnull >= 0) {
            dup2(devnull, STDOUT_FILENO);
            dup2(devnull, STDERR_FILENO);
            close(devnull);
        }

        if (command != NULL) {
            /* tmux new-session -d -s name -x cols -y rows command */
            execlp("tmux", "tmux", "new-session", "-d", "-s", name,
                   "-x", colsStr, "-y", rowsStr, command, (char*)NULL);
        } else {
            execlp("tmux", "tmux", "new-session", "-d", "-s", name,
                   "-x", colsStr, "-y", rowsStr, (char*)NULL);
        }
        _exit(1);
    }

    int status;
    waitpid(pid, &status, 0);
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}
