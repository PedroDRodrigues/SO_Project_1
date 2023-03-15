#include "fs/operations.h"
#include <assert.h>
#include <string.h>
#include <pthread.h>

void* tfs_read_1()
{
    char *string_aux = "Escreve isto...";
    char *entry_1 = "/f1";
    char terminal[50];

    int r = tfs_open(entry_1, TFS_O_CREAT);
    assert(r != -1);

    ssize_t write = tfs_write(r, string_aux, strlen(string_aux));
    assert(write == strlen(string_aux));

    assert(tfs_close(r) != -1);

    r = tfs_open(entry_1, 0);
    assert(r != -1);

    ssize_t read = tfs_read(r, terminal, sizeof(terminal) - 1);
    assert(read != -1);
    terminal[read] = '\0';
    assert(strcmp(terminal, string_aux) == 0);

    assert(tfs_close(r) != -1);
    
    return NULL;
}

int main() {

    pthread_t thread_1[5];

    assert(tfs_init() != -1);

    int i = 0;

    while (i < 5)
    {
        if (pthread_create(&thread_1[i], NULL, tfs_read_1, NULL) != 0)
            return -1;
        i++;
    }

    i = 0;

    while (i < 5)
    {
        if (pthread_join(thread_1[i], NULL) != 0)
            return -1;
        i++;
    }

    printf("Successful test.\n");

    return 0;
}
