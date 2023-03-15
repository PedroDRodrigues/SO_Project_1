#include "operations.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

int tfs_init() {
    state_init();

    /* create root inode */
    int root = inode_create(T_DIRECTORY);
    if (root != ROOT_DIR_INUM) {
        return -1;
    }

    return 0;
}

int tfs_destroy() {
    state_destroy();
    return 0;
}

static bool valid_pathname(char const *name) {
    return name != NULL && strlen(name) > 1 && name[0] == '/';
}

int tfs_lookup(char const *name) {
    if (!valid_pathname(name)) {
        return -1;
    }

    // skip the initial '/' character
    name++;

    return find_in_dir(ROOT_DIR_INUM, name);
}

int tfs_open(char const *name, int flags) {
    int inum;
    size_t offset;

    /* Checks if the path name is valid */
    if (!valid_pathname(name)) {
        return -1;
    }

    inum = tfs_lookup(name);
    if (inum >= 0) {
        /* The file already exists */
        inode_t *inode = inode_get(inum);
        if (inode == NULL) {
            return -1;
        }
        /* Truncate (if requested) */
        if (flags & TFS_O_TRUNC) {
            if (inode_data_free(inum) == -1) return -1;
            inode_metadata_reset(inum);
        }

        /* Determine initial offset */
        if (flags & TFS_O_APPEND) {
            /* Bloqueia o trinco read write do inode em read. */
            pthread_rwlock_rdlock(&inode->i_lock);
            offset = inode->i_size;
            /* Desloqueia o trinco read write do inode. */
            pthread_rwlock_unlock(&inode->i_lock);
        } else {
            offset = 0;
        }
    } else if (flags & TFS_O_CREAT) {
        /* The file doesn't exist; the flags specify that it should be created*/
        /* Create inode */
        inum = inode_create(T_FILE);
        if (inum == -1) {
            return -1;
        }
        /* Add entry in the root directory */
        if (add_dir_entry(ROOT_DIR_INUM, inum, name + 1) == -1) {
            inode_delete(inum);
            return -1;
        }
        offset = 0;
    } else {
        return -1;
    }

    /* Finally, add entry to the open file table and
     * return the corresponding handle */
    return add_to_open_file_table(inum, offset);

    /* Note: for simplification, if file was created with TFS_O_CREAT and there
     * is an error adding an entry to the open file table, the file is not
     * opened but it remains created */
}


int tfs_close(int fhandle) { return remove_from_open_file_table(fhandle); }

ssize_t tfs_write(int fhandle, void const *buffer, size_t to_write) {
    open_file_entry_t *file = get_open_file_entry(fhandle);
    if (file == NULL) {
        return -1;
    }

    size_t to_write_aux = 0;
    size_t to_write_receiver = to_write;

    /* From the open file table entry, we get the inode */
    inode_t *inode = inode_get(file->of_inumber);
    if (inode == NULL) {
        return -1;
    }

    /* Determine how many bytes to write */
    while (to_write > 0) {
        if (to_write + file->of_offset > BLOCK_SIZE) {
            to_write_aux = BLOCK_SIZE - file->of_offset;
        } else {
            to_write_aux = to_write;
        }
         
        if (to_write_aux > 0) {
            char *position = inode_update(file);
            if (position == NULL) {
                return -1;
            }
            /* Bloqueia o trinco read write do inode em write. */
            pthread_rwlock_wrlock(&inode->i_lock);
            /* Perform the actual write */
            memcpy(position, buffer, to_write_aux);
            /* Desloqueia o trinco read write do inode. */
            pthread_rwlock_unlock(&inode->i_lock);
            /* Avança o buffer */
            buffer += to_write_aux;
        }
    
        /* The offset associated with the file handle is
        * incremented accordingly */
       /* Bloqueia o trinco  da open file entry. */
        pthread_mutex_lock(&file->of_mutex);
        file->of_offset += to_write_aux;
        if (file->of_offset >= BLOCK_SIZE ) {
            file->of_offset -= BLOCK_SIZE;
            file->of_boffset++;
        }
        /* Desloqueia o trinco da open file entry . */
        pthread_mutex_unlock(&file->of_mutex);
        /* Bloqueia o trinco read write do inode em write. */
        pthread_rwlock_wrlock(&inode->i_lock);
        inode->i_size += to_write_aux;
        /* Desloqueia o trinco read write do inode. */
        pthread_rwlock_unlock(&inode->i_lock);
        to_write -= to_write_aux;
    }
    return (ssize_t)to_write_receiver;
}

ssize_t tfs_read(int fhandle, void *buffer, size_t len) {
    open_file_entry_t *file = get_open_file_entry(fhandle);
    if (file == NULL) {
        return -1;
    }
    /* From the open file table entry, we get the inode */
    inode_t *inode = inode_get(file->of_inumber);
    if (inode == NULL) {
        return -1;
    }
    
    /* Bloqueia o trinco  da open file entry. */
    pthread_mutex_lock(&file->of_mutex);
    /* Bloqueia o trinco read write do inode em read. */
    pthread_rwlock_rdlock(&inode->i_lock);
    /* Determine how many bytes to read */
    size_t to_read = inode->i_size - file->of_offset;
    /* Desloqueia o trinco read write do inode. */
    pthread_rwlock_unlock(&inode->i_lock);
    /* Desloqueia o trinco da open file entry . */
    pthread_mutex_unlock(&file->of_mutex);
    if (to_read > len) {
        to_read = len;
    }

    size_t to_read_aux;
    size_t to_read_receiver = to_read;
    while (to_read > 0) {
        /* Bloqueia o trinco  da open file entry. */
        pthread_mutex_lock(&file->of_mutex);
        if (file->of_offset + to_read >= BLOCK_SIZE) {
            to_read_aux = BLOCK_SIZE - file->of_offset;
        } else {
            to_read_aux = to_read;
        }
        /* Desloqueia o trinco da open file entry . */
        pthread_mutex_unlock(&file->of_mutex);

        if (to_read_aux > 0) {
            char *position = inode_update(file);
            if (position == NULL) {
                return -1;
            }

            /* Bloqueia o trinco read write do inode em read. */
            pthread_rwlock_rdlock(&inode->i_lock);
            /* Perform the actual read */
            memcpy(buffer, position, to_read);
            /* Desloqueia o trinco read write do inode. */
            pthread_rwlock_unlock(&inode->i_lock);
            /* Avança o buffer */
            buffer += to_read_aux;
        }
       /* Bloqueia o trinco  da open file entry. */
        pthread_mutex_lock(&file->of_mutex);
        file->of_offset += to_read_aux;
        if (file->of_offset >= BLOCK_SIZE ) {
            file->of_offset -= BLOCK_SIZE;
            file->of_boffset++;
        }
        /* Desloqueia o trinco da open file entry . */
        pthread_mutex_unlock(&file->of_mutex);
        to_read -= to_read_aux;
    }

    return (ssize_t)to_read_receiver;
}

int tfs_copy_to_external_fs(char const *source_path, char const *dest_path) {
    int source = tfs_open(source_path, 0);
    if (source == -1) {
        return -1;
    }

    open_file_entry_t *file_source = get_open_file_entry(source);
    if (file_source == NULL) {
        return -1;
    }

    inode_t *inode = inode_get(file_source->of_inumber);
    if (inode == NULL) {
        return -1;
    }
    /* Bloqueia o trinco read write do inode em read. */
    pthread_rwlock_rdlock(&inode->i_lock);
    size_t size = inode->i_size;
    /* Desloqueia o trinco read write do inode. */
    pthread_rwlock_unlock(&inode->i_lock);
    char buffer[size]; /* tamanho da array está definido por uma variável */
    size_t buffer_size = sizeof(buffer); /* Isto já era o size */

    size_t read = (size_t)tfs_read(source, buffer, buffer_size);
    if (read != size) {
        return -1;
    }
    
    FILE* dest = fopen(dest_path, "w");
    if (dest == NULL) {
        return -1;
    }
    if (fwrite(buffer, 1, read, dest) != size) {
        return -1;
    }
    fclose(dest);
    if (tfs_close(source) == -1) {
        return -1;
    }
    return 0;
}