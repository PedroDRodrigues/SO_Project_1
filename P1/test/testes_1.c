#include "fs/operations.h"
#include <string.h>
#include <pthread.h>
#include <assert.h>

void *tfs_open_1()
{
    char *entry_1 = "/f1";
    int entry_2 = TFS_O_CREAT;
    int f = tfs_open(entry_1, entry_2);
    assert(f != -1);
    assert(tfs_close(f) != -1);
    return NULL;
}

void *tfs_write_1()
{
    char *entry_1 = "/f1";
    char *entry_3 = "ABC DEF GHI JKL";
    int entry_2 = TFS_O_CREAT;
    int w = tfs_open(entry_1, entry_2);
    assert(w != -1);
    ssize_t write = tfs_write(w, entry_3, strlen(entry_3));
    assert(write == strlen(entry_3));
    assert(tfs_close(w) != -1);
    return NULL;
}

void *tfs_read_1()
{
    char *entry_1 = "/f1";
    char terminal[50];
    int entry_2 = TFS_O_CREAT;
    int o = tfs_open(entry_1, entry_2);
    assert(o != -1);
    ssize_t r = tfs_read(o, terminal, sizeof(terminal) - 1);
    assert(r == sizeof(terminal) -1);
    assert(tfs_close(o) != -1);
    return NULL;
}

int main() {

    pthread_t thread_1[15];

    assert(tfs_init() != -1);
    
    int i = 0;

    while (i < 5) 
    {
        if (pthread_create(&thread_1[i], NULL, tfs_open_1, NULL) != 0)
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

    i = 5;

    while (i < 10) 
    {
        if (pthread_create(&thread_1[i], NULL, tfs_write_1, NULL) != 0)
            return -1;
        i++;
    }
    
    i = 5;

    while (i < 10)
    {
        if (pthread_join(thread_1[i], NULL) != 0)
            return -1;
        i++;
    }

    i = 10;

    while (i < 15) 
    {
        if (pthread_create(&thread_1[i], NULL, tfs_read_1, NULL) != 0)
            return -1;
        i++;
    }
    
    i = 10;
    
    while (i < 15)
    {
        if (pthread_join(thread_1[i], NULL) != 0)
            return -1;
        i++;
    }

    printf("Successful test.\n");

    return 0;
}
