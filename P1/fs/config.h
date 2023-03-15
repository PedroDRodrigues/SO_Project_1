#ifndef CONFIG_H
#define CONFIG_H

/* FS root inode number */
#define ROOT_DIR_INUM (0)

#define BLOCK_SIZE (1024)
#define DATA_BLOCKS (1024)
#define INODE_TABLE_SIZE (50)
#define MAX_OPEN_FILES (20)
#define MAX_FILE_NAME (40)

/* Criadas */
#define INDEX_SIZE (sizeof(int))
#define INODE_BLOCK_POINTERS (11)
#define DIRECT_BLOCK_POINTERS (10)
#define MAX_BLOCK_POINTERS (BLOCK_SIZE/INDEX_SIZE)
#define EMPTY (-1)
#define MAX_FILE_SIZE ((MAX_BLOCK_POINTERS + DIRECT_BLOCK_POINTERS)*BLOCK_SIZE)
/* Fim das Criadas */

#define DELAY (5000)

#endif // CONFIG_H
