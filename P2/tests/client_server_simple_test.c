#include "client/tecnicofs_client_api.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/*  This test is similar to test1.c from the 1st exercise.
    The main difference is that this one explores the
    client-server architecture of the 2nd exercise. */

int main(int argc, char **argv) {

    char *str = "AAA!";
    char *path = "/f1";
    char buffer[40];

    int f;
    ssize_t r;

    if (argc < 3) {
        printf("You must provide the following arguments: 'client_pipe_path "
               "server_pipe_path'\n");
        return 1;
    }

    assert(tfs_mount(argv[1], argv[2]) == 0);
    printf("mount done\n");
    fflush(stdout);

    f = tfs_open(path, TFS_O_CREAT);
    printf("open 1 done\n");
    printf("%d\n",f);
    fflush(stdout);
    assert(f != -1);

    r = tfs_write(f, str, strlen(str));
    printf("write done\n");
    printf("%ld\n",r);
    fflush(stdout);
    assert(r == strlen(str));

    assert(tfs_close(f) != -1);
    printf("close 1 done\n");
    fflush(stdout);

    f = tfs_open(path, 0);
    printf("open 2 done\n");
    fflush(stdout);
    assert(f != -1);

    r = tfs_read(f, buffer, sizeof(buffer) - 1);
    printf("read done\n");
    fflush(stdout);
    assert(r == strlen(str));

    buffer[r] = '\0';
    assert(strcmp(buffer, str) == 0);

    assert(tfs_close(f) != -1);
    printf("close 2 done\n");
    fflush(stdout);

    assert(tfs_unmount() == 0);
    printf("unmount done\n");
    fflush(stdout);

    printf("Successful test.\n");

    return 0;
}
