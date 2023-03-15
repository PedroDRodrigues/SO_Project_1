#include "fs/operations.h"
#include <assert.h>
#include <string.h>
#include <pthread.h>

void *tfs_write_1()
{
    char *path = "/f1";
    char *entry_1 = "A a VER ver SE se ESTE este CORRE corre BEM bem!";
    int f = tfs_open(path, TFS_O_CREAT);
    assert(f != 1);
    ssize_t write = tfs_write(f, entry_1, strlen(entry_1));
    assert(write == strlen(entry_1));
    assert(tfs_close(f) != -1);
    return NULL;
}

void *tfs_write_2()
{
    char *path = "/f1";
    char *entry_1 = "Com a introducao de 1, 2 e 3!";
    int f = tfs_open(path, TFS_O_CREAT);
    assert(f != 1);
    ssize_t write = tfs_write(f, entry_1, strlen(entry_1));
    assert(write == strlen(entry_1));
    assert(tfs_close(f) != -1);
    return NULL;
}

void *tfs_write_3()
{
    char *path = "/f1";
    char *entry_1 = "Com elementos mais esquisitos como = e - ;)";
    int f = tfs_open(path, TFS_O_CREAT);
    assert(f != 1);
    ssize_t write = tfs_write(f, entry_1, strlen(entry_1));
    assert(write == strlen(entry_1));
    assert(tfs_close(f) != -1);
    return NULL;
}

void *tfs_write_4()
{
    char *path = "/f1";
    char *entry_1 = "Com eles todos misturados ^^ 2 ~~ 4.";
    int f = tfs_open(path, TFS_O_CREAT);
    assert(f != 1);
    ssize_t write = tfs_write(f, entry_1, strlen(entry_1));
    assert(write == strlen(entry_1));
    assert(tfs_close(f) != -1);
    return NULL;
}

void *tfs_write_5()
{
    char *path = "/f1";
    char *entry_1 = "o ultimo leva tudo igual";
    int f = tfs_open(path, TFS_O_CREAT);
    assert(f != 1);
    ssize_t write = tfs_write(f, entry_1, strlen(entry_1));
    assert(write == strlen(entry_1));
    assert(tfs_close(f) != -1);
    return NULL;
}

int main() {


    pthread_t thread_1[5];

    assert(tfs_init() != -1);

    if (pthread_create(&thread_1[0], NULL, tfs_write_1, NULL) != 0)
        return -1;

    if (pthread_join(thread_1[0], NULL) != 0)
            return -1;

    if (pthread_create(&thread_1[1], NULL, tfs_write_2, NULL) != 0)
        return -1;
    
    if (pthread_join(thread_1[1], NULL) != 0)
        return -1;

    if (pthread_create(&thread_1[2], NULL, tfs_write_3, NULL) != 0)
        return -1;
    
    if (pthread_join(thread_1[2], NULL) != 0)
        return -1;

    if (pthread_create(&thread_1[3], NULL, tfs_write_4, NULL) != 0)
        return -1;
    
    if (pthread_join(thread_1[3], NULL) != 0)
        return -1;

    if (pthread_create(&thread_1[4], NULL, tfs_write_5, NULL) != 0)
        return -1;
    
    if (pthread_join(thread_1[4], NULL) != 0)
        return -1;

    printf("Successful test.\n");

    return 0;
}
