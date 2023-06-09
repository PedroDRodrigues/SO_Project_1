#include "state.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

/* Persistent FS state  (in reality, it should be maintained in secondary
 * memory; for simplicity, this project maintains it in primary memory) */

/* I-node table */
static pthread_mutex_t it_mutex; /* trinco mutex para i-node table */
static inode_t inode_table[INODE_TABLE_SIZE];
static char freeinode_ts[INODE_TABLE_SIZE];

/* Data blocks */
static pthread_mutex_t db_mutex; /* trinco mutex para data blocks */
static char fs_data[BLOCK_SIZE * DATA_BLOCKS];
static char free_blocks[DATA_BLOCKS];

/* Volatile FS state */
static pthread_mutex_t vs_mutex; /* trinco mutex para volatile state */
static open_file_entry_t open_file_table[MAX_OPEN_FILES];
static char free_open_file_entries[MAX_OPEN_FILES];

static inline bool valid_inumber(int inumber) {
    return inumber >= 0 && inumber < INODE_TABLE_SIZE;
}

static inline bool valid_block_number(int block_number) {
    return block_number >= 0 && block_number < DATA_BLOCKS;
}

static inline bool valid_file_handle(int file_handle) {
    return file_handle >= 0 && file_handle < MAX_OPEN_FILES;
}

/**
 * We need to defeat the optimizer for the insert_delay() function.
 * Under optimization, the empty loop would be completely optimized away.
 * This function tells the compiler that the assembly code being run (which is
 * none) might potentially change *all memory in the process*.
 *
 * This prevents the optimizer from optimizing this code away, because it does
 * not know what it does and it may have side effects.
 *
 * Reference with more information: https://youtu.be/nXaxk27zwlk?t=2775
 *
 * Exercise: try removing this function and look at the assembly generated to
 * compare.
 */
static void touch_all_memory() { __asm volatile("" : : : "memory"); }

/*
 * Auxiliary function to insert a delay.
 * Used in accesses to persistent FS state as a way of emulating access
 * latencies as if such data structures were really stored in secondary memory.
 */
static void insert_delay() {
    for (int i = 0; i < DELAY; i++) {
        touch_all_memory();
    }
}

/*
 * Initializes FS state
 */
void state_init() {
    for (size_t i = 0; i < INODE_TABLE_SIZE; i++) {
        freeinode_ts[i] = FREE;
    }

    for (size_t i = 0; i < DATA_BLOCKS; i++) {
        free_blocks[i] = FREE;
    }

    for (size_t i = 0; i < MAX_OPEN_FILES; i++) {
        free_open_file_entries[i] = FREE;
    }

    /* Inicializa todos os trincos em state.c */
    pthread_mutex_init(&it_mutex, NULL);
    pthread_mutex_init(&db_mutex, NULL);
    pthread_mutex_init(&vs_mutex, NULL);

}

void state_destroy() { /* nothing to do */
/* Destrói todos os trincos */
pthread_mutex_destroy(&it_mutex);
pthread_mutex_destroy(&db_mutex);
pthread_mutex_destroy(&vs_mutex);
}

/*
 * Creates a new i-node in the i-node table.
 * Input:
 *  - n_type: the type of the node (file or directory)
 * Returns:
 *  new i-node's number if successfully created, -1 otherwise
 */
int inode_create(inode_type n_type) {
    for (int inumber = 0; inumber < INODE_TABLE_SIZE; inumber++) {
        if ((inumber * (int) sizeof(allocation_state_t)) == 0) {
            insert_delay(); // simulate storage access delay (to freeinode_ts)
        }
        /* Bloqueia o trinco da tabela de inodes. */
        pthread_mutex_lock(&it_mutex);
        /* Finds first free entry in i-node table */
        if (freeinode_ts[inumber] == FREE) {
            /* Found a free entry, so takes it for the new i-node*/
            freeinode_ts[inumber] = TAKEN;
            /* Desbloqueia o trinco da tabela de inodes. */
            pthread_mutex_unlock(&it_mutex);
            insert_delay(); // simulate storage access delay (to i-node)
            inode_table[inumber].i_node_type = n_type;

            if (n_type == T_DIRECTORY) {
                /* Initializes directory (filling its block with empty
                 * entries, labeled with inumber==-1) */
                int b = data_block_alloc();
                if (b == -1) {
                    /* Bloqueia o trinco da tabela de inodes. */
                    pthread_mutex_lock(&it_mutex);
                    /* Liberta espaço na tabela de inodes e destrói trinco read write do inode */
                    freeinode_ts[inumber] = FREE;
                    /* Desbloqueia o trinco da tabela de inodes. */
                    pthread_mutex_unlock(&it_mutex);
                    return -1;
                }

                inode_table[inumber].i_size = BLOCK_SIZE;
                inode_table[inumber].i_data_block = b;

                dir_entry_t *dir_entry = (dir_entry_t *)data_block_get(b);
                if (dir_entry == NULL) {
                    /* Liberta espaço na tabela de inodes e destrói trinco read write do inode */
                    freeinode_ts[inumber] = FREE;
                    return -1;
                }

                for (size_t i = 0; i < MAX_DIR_ENTRIES; i++) {
                    dir_entry[i].d_inumber = -1;
                }
            } else {
                /* In case of a new file, simply sets its size to 0 */
                inode_table[inumber].i_size = 0;
                inode_table[inumber].i_data_block = -1;
            }
            /* Define o valor dos indexes de blocos diretos como -1 */
            for(int i = 0; i < DIRECT_BLOCK_POINTERS; i++) {
                inode_table[inumber].i_direct_blocks[i] = -1;
            }
            return inumber;
        }
        /* Desbloqueia o trinco da tabela de inodes. */
        pthread_mutex_unlock(&it_mutex);
    }

    return -1;
}

/*
 * Deletes the i-node.
 * Input:
 *  - inumber: i-node's number
 * Returns: 0 if successful, -1 if failed
 */
int inode_delete(int inumber) {
    // simulate storage access delay (to i-node and freeinode_ts)
    insert_delay();
    insert_delay();

    if (!valid_inumber(inumber) || freeinode_ts[inumber] == FREE) {
        return -1;
    }

    freeinode_ts[inumber] = FREE;
    if (inode_data_free(inumber) == -1) return -1;
    

    /* TODO: handle non-empty directories (either return error, or recursively
     * delete children */
    return 0;

}

/*
 * Liberta todos os dados contidos no inode (auxiliar a inode_delete()) (funcionalidade do truncate).
 * Input:
 *  - inumber: i-node's number
 * Returns: 0 if successful, -1 if failed
 */
int inode_data_free(int inumber){
    if (inode_table[inumber].i_size > 0) {
        /* Bloqueia o trinco read write do inode em write. */
        pthread_rwlock_wrlock(&inode_table[inumber].i_lock);
        /* Frees all directly allocated blocks and sets each pointer to -1 */
        for(int i = 0; i < DIRECT_BLOCK_POINTERS; i++) {
            if (inode_table[inumber].i_direct_blocks[i] != -1){
                if (data_block_free(inode_table[inumber].i_direct_blocks[i]) == -1) {
                    /* Desloqueia o trinco read write do inode. */
                    pthread_rwlock_unlock(&inode_table[inumber].i_lock);
                    return -1;
                }
            }
        }
        /* Frees all indirectly alocated blocks and frees pointer block*/
        if (inode_table[inumber].i_data_block != -1){
            int *content = (int *)data_block_get(inode_table[inumber].i_data_block);
            if (content == NULL) {
                /* Desloqueia o trinco read write do inode. */
                pthread_rwlock_unlock(&inode_table[inumber].i_lock);
                return -1;
            }
            for (int i = 0; i < MAX_BLOCK_POINTERS; i++) {
                if (content[i] != -1){
                    if (data_block_free(content[i]) == -1) {
                        /* Desloqueia o trinco read write do inode. */
                        pthread_rwlock_unlock(&inode_table[inumber].i_lock);
                        return -1;
                    }
                }
            }
            if (data_block_free(inode_table[inumber].i_data_block) == -1) {
                /* Desloqueia o trinco read write do inode. */
                pthread_rwlock_unlock(&inode_table[inumber].i_lock);
                return -1;
            }
        }
    }
    /* Desloqueia o trinco read write do inode. */
    pthread_rwlock_unlock(&inode_table[inumber].i_lock);
    return 0;
}

/* Repõe os valores iniciais dos elementos do inode.
 * Input:
 *  - inumber: i-node's number
 */
void inode_metadata_reset(int inumber){
    /* Bloqueia o trinco read write do inode em write. */
    pthread_rwlock_wrlock(&inode_table[inumber].i_lock);
    for (int i = 0; i < DIRECT_BLOCK_POINTERS; i++)
    {
        inode_table[inumber].i_direct_blocks[i] = -1;
    }
    inode_table[inumber].i_data_block = -1;
    inode_table[inumber].i_size = 0;
    /* Desloqueia o trinco read write do inode. */
    pthread_rwlock_unlock(&inode_table[inumber].i_lock);
}

/*
 * Returns a pointer to an existing i-node.
 * Input:
 *  - inumber: identifier of the i-node
 * Returns: pointer if successful, NULL if failed
 */
inode_t *inode_get(int inumber) {
    if (!valid_inumber(inumber)) {
        return NULL;
    }

    insert_delay(); // simulate storage access delay to i-node
    
    return &inode_table[inumber];
}

/*
 * Updates block allocation of inode according to its offset in the open file entry.
 * Input:
 *  - file: pointer to the open file entry
 * Returns: pointer to the data referenced in the open file entry's offset, NULL if failed
 */
char *inode_update( open_file_entry_t *file) {
    /* Bloqueia o trinco  da open file entry. */
    pthread_mutex_lock(&file->of_mutex);
    /* From the open file table entry, we get the inode */
    inode_t *inode = inode_get(file->of_inumber);
    if (inode == NULL) {
        /* Desloqueia o trinco da open file entry . */
        pthread_mutex_unlock(&file->of_mutex);
        return NULL;
        
    }
    /* Bloqueia o trinco read write do inode em write. */
    pthread_rwlock_wrlock(&inode->i_lock);

    int blocks = file->of_boffset;
    char *position;

    for (int i = 0; i <= blocks && i < DIRECT_BLOCK_POINTERS; i++)
    {
        if (inode->i_direct_blocks[i] == -1)
        {
            inode->i_direct_blocks[i] = data_block_alloc();
        }
        
    }

    if (blocks >= DIRECT_BLOCK_POINTERS)
    {
        if (inode->i_data_block == -1) {
            inode->i_data_block = pointer_block_alloc();
        }
        blocks -= DIRECT_BLOCK_POINTERS;
        int *bpointer = (int *)data_block_get(inode->i_data_block);
        for (int i = 0; i <= blocks && i < MAX_BLOCK_POINTERS; i++)
        {
            if (bpointer[i] == -1)
            {
                bpointer[i] = data_block_alloc();
            }
            
        }
        /* Desloqueia o trinco read write do inode. */
        pthread_rwlock_unlock(&inode->i_lock);
        position = (char *)data_block_get(bpointer[blocks]);
        position += file->of_offset;
        /* Desloqueia o trinco da open file entry . */
        pthread_mutex_unlock(&file->of_mutex);
        return position;
    }
    /* Desloqueia o trinco read write do inode. */
    pthread_rwlock_unlock(&inode->i_lock);
    position = (char *)data_block_get(inode->i_direct_blocks[blocks]);
    position += file->of_offset;
    /* Desloqueia o trinco da open file entry . */
    pthread_mutex_unlock(&file->of_mutex);
    return position;
}

/*
 * Adds an entry to the i-node directory data.
 * Input:
 *  - inumber: identifier of the i-node
 *  - sub_inumber: identifier of the sub i-node entry
 *  - sub_name: name of the sub i-node entry
 * Returns: SUCCESS or FAIL
 */
int add_dir_entry(int inumber, int sub_inumber, char const *sub_name) {
    if (!valid_inumber(inumber) || !valid_inumber(sub_inumber)) {
        return -1;
    }

    insert_delay(); // simulate storage access delay to i-node with inumber


    if (inode_table[inumber].i_node_type != T_DIRECTORY) {
        return -1;
    }

    if (strlen(sub_name) == 0) {
        return -1;
    }

    /* Locates the block containing the directory's entries */
    dir_entry_t *dir_entry = (dir_entry_t *)data_block_get(inode_table[inumber].i_data_block); /* Isto está de acordo com a nova data struct? */

    if (dir_entry == NULL) {
        return -1;
    }

    /* Finds and fills the first empty entry */
    for (size_t i = 0; i < MAX_DIR_ENTRIES; i++) {
        if (dir_entry[i].d_inumber == -1) {

            dir_entry[i].d_inumber = sub_inumber;
            strncpy(dir_entry[i].d_name, sub_name, MAX_FILE_NAME - 1);
            dir_entry[i].d_name[MAX_FILE_NAME - 1] = 0;

            return 0;
        }
    }

    return -1;
}

/* Looks for a given name inside a directory
 * Input:
 * 	- parent directory's i-node number
 * 	- name to search
 * 	Returns i-number linked to the target name, -1 if not found
 */
int find_in_dir(int inumber, char const *sub_name) {
    insert_delay(); // simulate storage access delay to i-node with inumber

    if (!valid_inumber(inumber) ||
        inode_table[inumber].i_node_type != T_DIRECTORY) {
        return -1;
    }

    /* Locates the block containing the directory's entries */
    dir_entry_t *dir_entry =
        (dir_entry_t *)data_block_get(inode_table[inumber].i_data_block);
    if (dir_entry == NULL) {
        return -1;
    }

    /* Iterates over the directory entries looking for one that has the target
     * name */
    for (int i = 0; i < MAX_DIR_ENTRIES; i++)
        if ((dir_entry[i].d_inumber != -1) &&
            (strncmp(dir_entry[i].d_name, sub_name, MAX_FILE_NAME) == 0)) {

            return dir_entry[i].d_inumber;
        }

    return -1;
}

/*
 * Allocated a new data block
 * Returns: block index if successful, -1 otherwise
 */
int data_block_alloc() {
    for (int i = 0; i < DATA_BLOCKS; i++) {
        if (i * (int) sizeof(allocation_state_t) % BLOCK_SIZE == 0) {
            insert_delay(); // simulate storage access delay to free_blocks
        }
        /* Bloqueia trinco da tabela de data blocks. */
        pthread_mutex_lock(&db_mutex);
        if (free_blocks[i] == FREE) {
            free_blocks[i] = TAKEN;
            /* Desbloqueia trinco da tabela de data blocks. */
            pthread_mutex_unlock(&db_mutex);
            return i;
        }
        /* Desbloqueia trinco da tabela de data blocks. */
        pthread_mutex_unlock(&db_mutex);
    }
    return -1;
}
/*
 * Allocats a new pointer block (with all values set to 0)
 * Returns: block index if successful, -1 otherwise
 */
int pointer_block_alloc() {
    int index, *content;
    index = data_block_alloc();
    if (index == -1) {
        return -1;
    }
    content = data_block_get(index);
    if (content == NULL) {
        return -1;
    }
    for (int i = 0; i < MAX_BLOCK_POINTERS; i++) {
        content[i] = EMPTY;
    }
    return index;
}

/* Frees a data block
 * Input
 * 	- the block index
 * Returns: 0 if success, -1 otherwise
 */
int data_block_free(int block_number) {
    if (!valid_block_number(block_number)) {
        return -1;
    }

    insert_delay(); // simulate storage access delay to free_blocks
    /* Bloqueia o trinco da tabela de data blocks. */
    pthread_mutex_lock(&db_mutex);
    free_blocks[block_number] = FREE;
    /* Desbloqueia o trinco da tabela de data blocks. */
    pthread_mutex_unlock(&db_mutex);
    return 0;
}

/* Returns a pointer to the contents of a given block
 * Input:
 * 	- Block's index
 * Returns: pointer to the first byte of the block, NULL otherwise
 */
void *data_block_get(int block_number) {
    if (!valid_block_number(block_number)) {
        return NULL;
    }

    insert_delay(); // simulate storage access delay to block
    return &fs_data[block_number * BLOCK_SIZE];
}

/* Add new entry to the open file table
 * Inputs:
 * 	- I-node number of the file to open
 * 	- Initial offset
 * Returns: file handle if successful, -1 otherwise
 */
int add_to_open_file_table(int inumber, size_t offset) {
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        /* Bloqueia o trinco do FS volatile state */
        pthread_mutex_lock(&vs_mutex);
        if (free_open_file_entries[i] == FREE) {
            free_open_file_entries[i] = TAKEN;
            
            /* Desbloqueia o trinco do FS volatile state */
            pthread_mutex_unlock(&vs_mutex);

            open_file_table[i].of_inumber = inumber;

            int block_aux = (int) offset;
            block_aux = block_aux/BLOCK_SIZE;

            open_file_table[i].of_boffset = block_aux;

            offset = offset % BLOCK_SIZE;

            open_file_table[i].of_offset = offset;

            if (pthread_mutex_init(&open_file_table[i].of_mutex, NULL) == -1) return -1;
            return i;
        }
        /* Desbloqueia o trinco do FS volatile state */
        pthread_mutex_unlock(&vs_mutex);
    }
    return -1;
}

/* Frees an entry from the open file table
 * Inputs:
 * 	- file handle to free/close
 * Returns 0 is success, -1 otherwise
 */
int remove_from_open_file_table(int fhandle) {
    if (!valid_file_handle(fhandle) ||
        free_open_file_entries[fhandle] != TAKEN) {
        return -1;
    }
    /* Bloqueia o trinco do FS volatile state */
    pthread_mutex_lock(&vs_mutex);
    free_open_file_entries[fhandle] = FREE;
    /* Desbloqueia o trinco do FS volatile state */
    pthread_mutex_unlock(&vs_mutex);
    return 0;
}

/* Returns pointer to a given entry in the open file table
 * Inputs:
 * 	 - file handle
 * Returns: pointer to the entry if sucessful, NULL otherwise
 */
open_file_entry_t *get_open_file_entry(int fhandle) {
    if (!valid_file_handle(fhandle)) {
        return NULL;
    }
    return &open_file_table[fhandle];
}
