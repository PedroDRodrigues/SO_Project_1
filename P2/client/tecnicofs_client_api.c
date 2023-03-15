#include "tecnicofs_client_api.h"
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>

#define MAX_SIZE_MESSAGE (100)
#define MAX_PIPE_NAME (40)

int session_id;
int fclient, fserver;
char pipe_buffer[MAX_PIPE_NAME];

int tfs_mount(char const *client_pipe_path, char const *server_pipe_path) {
    char message_buffer[MAX_SIZE_MESSAGE] = "";
    ssize_t msg;

    for (int i = 0; i < MAX_PIPE_NAME; i++)
    {
        pipe_buffer[i] = '\0';
    }

    if (unlink(client_pipe_path) != 0 && errno != ENOENT) {
        fprintf(stderr, "[ERR]: client unlink(%s) failed: %s\n", client_pipe_path,
                strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (mkfifo(client_pipe_path, 0777) != 0) {
        fprintf(stderr, "[ERR]: client mkfifo failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    strcpy(pipe_buffer, client_pipe_path);

    do
    {
        fserver = open(server_pipe_path, O_WRONLY);
    } while (fserver == -1 && (errno == EINTR || errno == ENOENT));

    if (fserver == -1)
    {
        fprintf(stderr, "[ERR]: server open by client failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    memcpy(message_buffer, "1", sizeof(char));
    memcpy(message_buffer + sizeof(char), client_pipe_path, MAX_PIPE_NAME);

    do
    {
        msg = write(fserver, message_buffer, sizeof(message_buffer));
    } while (msg == -1 && errno == EINTR);

    if (msg == -1)
    {
        fprintf(stderr, "[ERR]: client write on server pipe failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    do
    {
        fclient = open(client_pipe_path, O_RDONLY);
    } while (fclient == -1 && errno == EINTR);

    if (fclient == -1)
    {
        fprintf(stderr, "[ERR]: client open by client failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    do
    {
        msg = read(fclient, &session_id, sizeof(int));
    } while (msg == -1 && errno == EINTR);

    if (msg == -1)
    {
        fprintf(stderr, "[ERR]: client read failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    return 0;
}

int tfs_unmount() {
    char message_buffer[MAX_SIZE_MESSAGE] = "";
    memcpy(message_buffer, "2", sizeof(char));
    memcpy(message_buffer + sizeof(char), &session_id, sizeof(int));
    ssize_t msg;

    do
    {
        msg = write(fserver, message_buffer, sizeof(message_buffer));
    } while (msg == -1 && errno == EINTR);

    if (msg == -1)
    {
        fprintf(stderr, "[ERR]: client write on server pipe failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (close(fclient) < 0)
        return -1;

    if (close(fserver) < 0)
        return -1;
    
    if (unlink(pipe_buffer) < 0)
        return -1;

    return 0;
}

int tfs_open(char const *name, int flags) {
    int res;
    char message_buffer[MAX_SIZE_MESSAGE] = "";
    memcpy(message_buffer, "3", sizeof(char));
    memcpy(message_buffer + sizeof(char), &session_id, sizeof(int));
    memcpy(message_buffer + sizeof(char) + sizeof(int), name, MAX_PIPE_NAME*sizeof(char));
    memcpy(message_buffer + sizeof(char) + sizeof(int) + sizeof(char), &flags, sizeof(int));
    ssize_t msg;

    do
    {
        msg = write(fserver, message_buffer, sizeof(message_buffer));
    } while (msg == -1 && errno == EINTR);

    if (msg == -1)
    {
        fprintf(stderr, "[ERR]: client write on server pipe failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    do
    {
        msg = read(fclient, &res, sizeof(int));
    } while (msg == -1 && errno == EINTR);

    if (msg == -1)
    {
        fprintf(stderr, "[ERR]: client read failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    return res;
}

int tfs_close(int fhandle) {
    int res;
    char message_buffer[MAX_SIZE_MESSAGE] = "";
    memcpy(message_buffer, "4", sizeof(char));
    memcpy(message_buffer + sizeof(char), &session_id, sizeof(int));
    memcpy(message_buffer + sizeof(char) + sizeof(int), &fhandle, sizeof(int));
    ssize_t msg;

    do
    {
        msg = write(fserver, message_buffer, sizeof(message_buffer));
    } while (msg == -1 && errno == EINTR);

    if (msg == -1)
    {
        fprintf(stderr, "[ERR]: client write on server pipe failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    do
    {
        msg = read(fclient, &res, sizeof(int));
    } while (msg == -1 && errno == EINTR);

    if (msg == -1)
    {
        fprintf(stderr, "[ERR]: client read failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    return res;
}

ssize_t tfs_write(int fhandle, void const *buffer, size_t len) {
    ssize_t res;
    char message_buffer[MAX_SIZE_MESSAGE] = "";
    memcpy(message_buffer, "5", sizeof(char));
    memcpy(message_buffer + sizeof(char), &session_id, sizeof(int));
    memcpy(message_buffer + sizeof(char) + sizeof(int), &fhandle, sizeof(int));
    memcpy(message_buffer + sizeof(char) + sizeof(int) + sizeof(int), &len, sizeof(size_t));
    memcpy(message_buffer + sizeof(char) + sizeof(int) + sizeof(int) + sizeof(size_t), buffer, len * sizeof(char));
    ssize_t msg;

    do
    {
        msg = write(fserver, message_buffer, sizeof(message_buffer));
    } while (msg == -1 && errno == EINTR);

    if (msg == -1)
    {
        fprintf(stderr, "[ERR]: client write on server pipe failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    do
    {
        msg = read(fclient, &res, sizeof(ssize_t));
    } while (msg == -1 && errno == EINTR);

    if (msg == -1)
    {
        fprintf(stderr, "[ERR]: client read failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    return res;
}

ssize_t tfs_read(int fhandle, void *buffer, size_t len) {
    ssize_t res;
    char message_buffer[MAX_SIZE_MESSAGE] = "";
    char receiver_buffer[MAX_SIZE_MESSAGE] = ""; 
    memcpy(message_buffer, "6", sizeof(char));
    memcpy(message_buffer + sizeof(char), &session_id, sizeof(int));
    memcpy(message_buffer + sizeof(char) + sizeof(int), &fhandle, sizeof(int));
    ssize_t msg;

    do
    {
        msg = write(fserver, message_buffer, sizeof(message_buffer));
    } while (msg == -1 && errno == EINTR);

    if (msg == -1)
    {
        fprintf(stderr, "[ERR]: client write on server pipe failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    do
    {
        msg = read(fclient, receiver_buffer, sizeof(ssize_t) + len*sizeof(char));
    } while (msg == -1 && errno == EINTR);

    if (msg == -1)
    {
        fprintf(stderr, "[ERR]: client read failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    memcpy(&res, receiver_buffer, sizeof(ssize_t));
    memcpy(buffer, receiver_buffer + sizeof(ssize_t), res * sizeof(char));

    return res;
}

int tfs_shutdown_after_all_closed() {
    int res;
    char message_buffer[MAX_SIZE_MESSAGE] = "";
    memcpy(message_buffer, "7", sizeof(char));
    memcpy(message_buffer + sizeof(char), &session_id, sizeof(int));
    ssize_t msg;

    do
    {
        msg = write(fserver, message_buffer, sizeof(message_buffer));
    } while (msg == -1 && errno == EINTR);

    if (msg == -1)
    {
        fprintf(stderr, "[ERR]: client write on server pipe failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    do
    {
        msg = read(fclient, &res, sizeof(int));
    } while (msg == -1 && errno == EINTR);

    if (msg == -1)
    {
        fprintf(stderr, "[ERR]: client read failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    return res;
}