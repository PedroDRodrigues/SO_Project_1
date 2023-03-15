#include "operations.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <signal.h>

#define S (10)
#define MAX_MESSAGE_SIZE (100)
#define MAX_PIPE_NAME (40)

enum {ON, OFF};

typedef struct commands
{
    int session_id;
    char pipename[MAX_PIPE_NAME];
    char op_code;
    char name[40];
    int fnum;
    size_t len;
    char* buf;

}command_t;

static int status; /* determines if the server is on or off */

static int session_status[S]; /* list of session statuses */
static command_t buffer[S]; /* producer-consumer buffers */
static int session_count;

static pthread_mutex_t m[S];	/* mutex locks for buffers */
static pthread_cond_t c_cons[S]; /* consumers wait on this cond var */

void *producer(void *pipename);
void *consumer(void *sid);

int main(int argc, char **argv) {
    pthread_t ct[S], pt;
    int i;

    signal(SIGPIPE,SIG_IGN);

    if (argc < 2) {
        printf("Please specify the pathname of the server's pipe.\n");
        return 1;
    }

    char *pipename = argv[1];
    printf("Starting TecnicoFS server with pipe called %s\n", pipename);

    if (unlink(pipename) != 0 && errno != ENOENT) {
        fprintf(stderr, "[ERR]: server unlink(%s) failed: %s\n", pipename,
                strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (mkfifo(pipename, 0777) < 0) {
        fprintf(stderr, "[ERR]: server mkfifo failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (tfs_init() != 0)
        {
        printf("[ERR]: tfs_init failed");
        return 1;
        }

    status = ON;
    session_count = 0;

    for (i = 0; i < S; i++)
    {
        session_status[i] = -1;
        buffer[i].session_id = i;
        if (pthread_mutex_init(&m[i], NULL) != 0)
            return 1;
        if (pthread_cond_init(&c_cons[i], NULL) != 0)
            return 1;
        if (pthread_create(&ct[i], NULL, consumer, &buffer[i]) != 0)
            return 1;
        
    }

    if (pthread_create(&pt, NULL, producer, pipename) != 0)
        return 1;

    if (pthread_join(pt, NULL) != 0)
        return 1;

    for (i = 0; i < S; i++)
    {
        if (pthread_join(ct[i], NULL) != 0)
            return 1;
    }

    for (i = 0; i < S; i++)
    {
        pthread_mutex_destroy(&m[i]);
        pthread_cond_destroy(&c_cons[i]);
    }

    return 0;
}

void *producer(void *pipename) {
    int spipe, cpipe;
    command_t cbuf;
    int r, vi;
    ssize_t vs;

    /* OPEN SERVER PIPE */
    do
    {
        spipe = open(pipename, O_RDONLY);
    } while (spipe == -1 && errno == EINTR);

    if (spipe == -1)
    {
        fprintf(stderr, "[ERR]: server open failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    /* READ AND PROCESS COMMANDS WHILE ON */
    while(status == ON)
    {
        /* READ OP_CODE */
        do
        {
            vs = read(spipe, &cbuf.op_code, sizeof(char));
        } while (vs == -1 && errno == EINTR);

        if (vs == -1)
        {
            fprintf(stderr, "[ERR]: server read failed: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }

        switch (cbuf.op_code)
        {
        case '1': /* MOUNT */
            /*READ PIPENAME */
            do
            {
                vs = read(spipe, cbuf.pipename, MAX_PIPE_NAME*sizeof(char));
            } while (vs == -1 && errno == EINTR);

            if (vs == -1)
            {
                fprintf(stderr, "[ERR]: server read failed: %s\n", strerror(errno));
                exit(EXIT_FAILURE);
            }
            /* CHECK IF SESSIONS ARE FULL */
            if (session_count >= S)
            {
                /* RETURN -1 TO CLIENT */
                r = -1;
                    /* OPEN CLIENT PIPE */
                do
                {
                    cpipe = open(cbuf.pipename, O_WRONLY);
                } while (cpipe == -1 && errno == EINTR);

                if (cpipe == -1)
                {
                    fprintf(stderr, "[ERR]: client pipe open by server failed: %s\n", strerror(errno));
                    exit(EXIT_FAILURE);
                }
                    /* WRITE RETURN VALUE ON CLIENT PIPE */
                    do
                    {
                        vs = write(cpipe,&r,sizeof(int));
                    } while (vs == -1 && errno == EINTR);

                    if (vs == -1)
                    {
                        fprintf(stderr, "[ERR]: server write on client pipe failed: %s\n", strerror(errno));
                        exit(EXIT_FAILURE);
                    }
                    /* CLOSE CLIENT PIPE */
                    do
                    {
                        vi = close(cpipe);
                    } while (vi == -1 && errno == EINTR);

                    if (vi == -1)
                    {
                        fprintf(stderr, "[ERR]: client pipe close by server failed: %s\n", strerror(errno));
                        exit(EXIT_FAILURE);
                    }
            } else /* IF THERE IS A FREE SESSION */
            {
                for (int i = 0; i < S; i++)
                {
                    if (session_status[i] == -1)
                    {
                        /* CHANGE SESSION STATUS */
                        session_status[i] = 0;
                        /* PASS OP_CODE TO COMMAND BUFFER */
                        buffer[i].op_code = '1';
                        /* REGISTER PIPE */
                        memcpy(buffer[i].pipename, cbuf.pipename, MAX_PIPE_NAME*sizeof(char));
                        /* CALL CONSUMER THREAD */
                        pthread_mutex_lock(&m[i]);
                        pthread_cond_signal(&c_cons[i]);
                        pthread_mutex_unlock(&m[i]);
                    }
                }
            }
            break;

        case '2': /* UNMOUNT */
            /* READ SESSION ID */
            do
            {
                vs = read(spipe, &cbuf.session_id, sizeof(int));
            } while (vs == -1 && errno == EINTR);

            if (vs == -1)
            {
                fprintf(stderr, "[ERR]: server read failed: %s\n", strerror(errno));
                exit(EXIT_FAILURE);
            }
            /* PASS INFORMATION TO COMMAND BUFFER */
            buffer[cbuf.session_id].op_code = '2';

            /* CALL CONSUMER THREAD */
            pthread_mutex_lock(&m[cbuf.session_id]);
            pthread_cond_signal(&c_cons[cbuf.session_id]);
            pthread_mutex_unlock(&m[cbuf.session_id]);
            
            break;
        
        case '3': /* OPEN */
            /* READ SESSION ID */
            do
            {
                vs = read(spipe, &cbuf.session_id, sizeof(int));
            } while (vs == -1 && errno == EINTR);

            if (vs == -1)
            {
                fprintf(stderr, "[ERR]: server read failed: %s\n", strerror(errno));
                exit(EXIT_FAILURE);
            }
            /* READ NAME */
            do
            {
                vs = read(spipe, buffer[cbuf.session_id].name, MAX_PIPE_NAME*sizeof(char));
            } while (vs == -1 && errno == EINTR);

            if (vs == -1)
            {
                fprintf(stderr, "[ERR]: server read failed: %s\n", strerror(errno));
                exit(EXIT_FAILURE);
            }
            /* READ FLAGS*/
            do
            {
                vs = read(spipe, &buffer[cbuf.session_id].fnum, sizeof(int));
            } while (vs == -1 && errno == EINTR);

            if (vs == -1)
            {
                fprintf(stderr, "[ERR]: server read failed: %s\n", strerror(errno));
                exit(EXIT_FAILURE);
            }
            /* PASS OP_CODE TO COMMAND BUFFER */
            buffer[cbuf.session_id].op_code = '3';

            /* CALL CONSUMER THREAD */
            pthread_mutex_lock(&m[cbuf.session_id]);
            pthread_cond_signal(&c_cons[cbuf.session_id]);
            pthread_mutex_unlock(&m[cbuf.session_id]);
            break;

        case '4': /* CLOSE */
            /* READ SESSION ID */
            do
            {
                vs = read(spipe, &cbuf.session_id, sizeof(int));
            } while (vs == -1 && errno == EINTR);

            if (vs == -1)
            {
                fprintf(stderr, "[ERR]: server read failed: %s\n", strerror(errno));
                exit(EXIT_FAILURE);
            }
            /* READ FHANDLE*/
            do
            {
                vs = read(spipe, &buffer[cbuf.session_id].fnum, sizeof(int));
            } while (vs == -1 && errno == EINTR);

            if (vs == -1)
            {
                fprintf(stderr, "[ERR]: server read failed: %s\n", strerror(errno));
                exit(EXIT_FAILURE);
            }
            /* PASS OP_CODE TO COMMAND BUFFER */
            buffer[cbuf.session_id].op_code = '4';

            /* CALL CONSUMER THREAD */
            pthread_mutex_lock(&m[cbuf.session_id]);
            pthread_cond_signal(&c_cons[cbuf.session_id]);
            pthread_mutex_unlock(&m[cbuf.session_id]);
            break;

        case '5': /* WRITE */
            /* READ SESSION ID */
            do
            {
                vs = read(spipe, &cbuf.session_id, sizeof(int));
            } while (vs == -1 && errno == EINTR);

            if (vs == -1)
            {
                fprintf(stderr, "[ERR]: server read failed: %s\n", strerror(errno));
                exit(EXIT_FAILURE);
            }
            /* READ FHANDLE*/
            do
            {
                vs = read(spipe, &buffer[cbuf.session_id].fnum, sizeof(int));
            } while (vs == -1 && errno == EINTR);

            if (vs == -1)
            {
                fprintf(stderr, "[ERR]: server read failed: %s\n", strerror(errno));
                exit(EXIT_FAILURE);
            }
            /* READ LEN*/
            do
            {
                vs = read(spipe, &buffer[cbuf.session_id].len, sizeof(size_t));
            } while (vs == -1 && errno == EINTR);

            if (vs == -1)
            {
                fprintf(stderr, "[ERR]: server read failed: %s\n", strerror(errno));
                exit(EXIT_FAILURE);
            }
            /* READ CONTENT*/
            buffer[cbuf.session_id].buf = (char*) malloc(buffer[cbuf.session_id].len);
            do
            {
                vs = read(spipe, buffer[cbuf.session_id].buf, buffer[cbuf.session_id].len);
            } while (vs == -1 && errno == EINTR);

            if (vs == -1)
            {
                fprintf(stderr, "[ERR]: server read failed: %s\n", strerror(errno));
                exit(EXIT_FAILURE);
            }
            /* PASS OP_CODE TO COMMAND BUFFER */
            buffer[cbuf.session_id].op_code = '5';

            /* CALL CONSUMER THREAD */
            pthread_mutex_lock(&m[cbuf.session_id]);
            pthread_cond_signal(&c_cons[cbuf.session_id]);
            pthread_mutex_unlock(&m[cbuf.session_id]);
            break;
        
        case '6': /* READ */
            /* READ SESSION ID */
            do
            {
                vs = read(spipe, &cbuf.session_id, sizeof(int));
            } while (vs == -1 && errno == EINTR);

            if (vs == -1)
            {
                fprintf(stderr, "[ERR]: server read failed: %s\n", strerror(errno));
                exit(EXIT_FAILURE);
            }
            /* READ FHANDLE*/
            do
            {
                vs = read(spipe, &buffer[cbuf.session_id].fnum, sizeof(int));
            } while (vs == -1 && errno == EINTR);

            if (vs == -1)
            {
                fprintf(stderr, "[ERR]: server read failed: %s\n", strerror(errno));
                exit(EXIT_FAILURE);
            }
            /* READ LEN*/
            do
            {
                vs = read(spipe, &buffer[cbuf.session_id].len, sizeof(size_t));
            } while (vs == -1 && errno == EINTR);

            if (vs == -1)
            {
                fprintf(stderr, "[ERR]: server read failed: %s\n", strerror(errno));
                exit(EXIT_FAILURE);
            }
            /* PASS OP_CODE TO COMMAND BUFFER */
            buffer[cbuf.session_id].op_code = '6';

            /* CALL CONSUMER THREAD */
            pthread_mutex_lock(&m[cbuf.session_id]);
            pthread_cond_signal(&c_cons[cbuf.session_id]);
            pthread_mutex_unlock(&m[cbuf.session_id]);
            break;

        case '7': /* SHUTDOWN */
            /* READ SESSION ID */
            do
            {
                vs = read(spipe, &cbuf.session_id, sizeof(int));
            } while (vs == -1 && errno == EINTR);

            if (vs == -1)
            {
                fprintf(stderr, "[ERR]: server read failed: %s\n", strerror(errno));
                exit(EXIT_FAILURE);
            }
            /* PASS OP_CODE TO COMMAND BUFFER */
            buffer[cbuf.session_id].op_code = '7';
            /* CALL CONSUMER THREAD */
            pthread_mutex_lock(&m[cbuf.session_id]);
            pthread_cond_signal(&c_cons[cbuf.session_id]);
            pthread_mutex_unlock(&m[cbuf.session_id]);
            break;
        
        default:
            break;
        }
    }

    /* CLOSE SERVER PIPE */
    do
    {
        r = close(spipe);
    } while (r == -1 && errno == EINTR);

    if (r == -1)
    {
        fprintf(stderr, "[ERR]: server pipe close by server failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    return NULL;
}

void *consumer(void *buffer) {
    int cpipe;
    int r;
    ssize_t msg, rt;
    command_t* command;

    command = (command_t *) buffer;

    while (status == ON) {
        /* I AWAIT YOUR COMMAND */
        pthread_mutex_lock(&m[command->session_id]);
        pthread_cond_wait(&c_cons[command->session_id],&m[command->session_id]);

        switch (command->op_code)
        {
            case '1': /* MOUNT */
                /* OPEN CLIENT PIPE */
                do
                {
                    cpipe = open(command->pipename,O_WRONLY);
                } while (cpipe == -1 && errno == EINTR);

                if (cpipe == -1)
                {

                    if (errno == EPIPE)
                    {
                        session_status[command->session_id] = -1;
                        session_count--;
                    } else
                    {
                        fprintf(stderr, "[ERR]: client pipe open by server failed: %s\n", strerror(errno));
                        exit(EXIT_FAILURE);
                    }
                }
                /* RETURN SESSION ID TO CLIENT */
                do
                {
                    msg = write(cpipe,&command->session_id,sizeof(int));
                } while (msg == -1 && errno == EINTR);

                if (msg == -1)
                {
                    if (errno == EPIPE)
                    {
                        close(session_status[command->session_id]);
                        session_status[command->session_id] = -1;
                        session_count--;
                    } else
                    {
                        fprintf(stderr, "[ERR]: client pipe write by server failed: %s\n", strerror(errno));
                        exit(EXIT_FAILURE);
                    }
                }
                break;

            case '2': /* UNMOUNT */
                /* CHANGE SESSION STATUS AND COUNT */
                session_count--;
                session_status[command->session_id] = -1;
                /* RETURN 0 TO CLIENT */
                r = 0;
                do
                {
                    msg = write(cpipe,&r,sizeof(int));
                } while (msg == -1 && errno == EINTR);

                if (msg == -1)
                {
                    fprintf(stderr, "[ERR]: server write on client pipe failed: %s\n", strerror(errno));
                    exit(EXIT_FAILURE);
                }
                /* CLOSE CLIENT PIPE */
                do
                {
                    r = close(cpipe);
                } while (r == -1 && errno == EINTR);

                if (r == -1)
                {
                    if (errno == EPIPE)
                    {
                        close(session_status[command->session_id]);
                        session_status[command->session_id] = -1;
                        session_count--;
                    } else
                    {
                        fprintf(stderr, "[ERR]: client pipe close by server failed: %s\n", strerror(errno));
                        exit(EXIT_FAILURE);
                    }
                }
                break;
            
            case '3': /* OPEN */
                /* CALL TFS_OPEN */
                r = tfs_open(command->name,command->fnum);
                /* RETURN RESULT TO CLIENT */
                do
                {
                    msg = write(cpipe,&r,sizeof(int));
                } while (msg == -1 && errno == EINTR);

                if (msg == -1)
                {
                    if (errno == EPIPE)
                    {
                        close(session_status[command->session_id]);
                        session_status[command->session_id] = -1;
                        session_count--;
                    } else
                    {
                        fprintf(stderr, "[ERR]: client pipe write by server failed: %s\n", strerror(errno));
                        exit(EXIT_FAILURE);
                    }
                }
                break;

            case '4': /* CLOSE */;
                /* CALL TFS_CLOSE */
                r = tfs_close(command->fnum);
                /* RETURN RESULT TO CLIENT */
                do
                {
                    msg = write(cpipe,&r,sizeof(int));
                } while (msg == -1 && errno == EINTR);

                if (msg == -1)
                {
                    if (errno == EPIPE)
                    {
                        close(session_status[command->session_id]);
                        session_status[command->session_id] = -1;
                        session_count--;
                    } else
                    {
                        fprintf(stderr, "[ERR]: client pipe write by server failed: %s\n", strerror(errno));
                        exit(EXIT_FAILURE);
                    }
                }
                break;

            case '5': /* WRITE */
                /* CALL TFS_WRITE */
                rt = tfs_write(command->fnum,command->buf,command->len);
                free(command->buf);
                /* RETURN RESULT TO CLIENT */
                do
                {
                    msg = write(cpipe,&rt,sizeof(ssize_t));
                } while (msg == -1 && errno == EINTR);

                if (msg == -1)
                {
                    if (errno == EPIPE)
                    {
                        close(session_status[command->session_id]);
                        session_status[command->session_id] = -1;
                        session_count--;
                    } else
                    {
                        fprintf(stderr, "[ERR]: client pipe write by server failed: %s\n", strerror(errno));
                        exit(EXIT_FAILURE);
                    }
                }
                break;

            case '6': /* READ */
                /* CALL TFS_READ */
                command->buf = (char*) malloc(command->len);
                rt = tfs_read(command->fnum,command->buf,command->len);
                /* RETURN RESULT TO CLIENT
                 * NUMBER OF READ BYTES */
                do
                {
                    msg = write(cpipe,&rt,sizeof(ssize_t));
                } while (msg == -1 && errno == EINTR);

                if (msg == -1)
                {
                    if (errno == EPIPE)
                    {
                        close(session_status[command->session_id]);
                        session_status[command->session_id] = -1;
                        session_count--;
                    } else
                    {
                        fprintf(stderr, "[ERR]: client pipe write by server failed: %s\n", strerror(errno));
                        exit(EXIT_FAILURE);
                    }
                }
                if (rt != -1)
                {
                    /* PREVIOUSLY READ CONTENT */
                    do
                    {
                        msg = write(cpipe,command->buf,command->len);
                    } while (msg == -1 && errno == EINTR);

                    if (msg == -1)
                    {
                        if (errno == EPIPE)
                    {
                        close(session_status[command->session_id]);
                        session_status[command->session_id] = -1;
                        session_count--;
                    } else
                    {
                        fprintf(stderr, "[ERR]: client pipe write by server failed: %s\n", strerror(errno));
                        exit(EXIT_FAILURE);
                    }
                    }
                }
                /* FREE BUF */
                free(command->buf);
                break;

            case '7': /* SHUTDOWN */
                /* CALL TFS_DESTROY_AFTER_ALL_CLOSED */
                r = tfs_destroy_after_all_closed();
                /* TURN OFF SERVER */
                status = OFF;
                /* RETURN RESULT TO CLIENT */
                do
                {
                    msg = write(cpipe,&r,sizeof(int));
                } while (msg == -1 && errno == EINTR);

                if (msg == -1)
                {
                    if (errno == EPIPE)
                    {
                        close(session_status[command->session_id]);
                        session_status[command->session_id] = -1;
                        session_count--;
                    } else
                    {
                        fprintf(stderr, "[ERR]: client pipe write by server failed: %s\n", strerror(errno));
                        exit(EXIT_FAILURE);
                    }
                }
                    /* CLOSE CLIENT PIPE */
                do
                {
                    r = close(cpipe);
                } while (r == -1 && errno == EINTR);

                if (r == -1)
                {
                    if (errno == EPIPE)
                    {
                        session_status[command->session_id] = -1;
                        session_count--;
                    } else
                    {
                        fprintf(stderr, "[ERR]: client pipe CLOSE by server failed: %s\n", strerror(errno));
                        exit(EXIT_FAILURE);
                    }
                }
                break;

            default:
                r = -1;
                break;
        }
        pthread_mutex_unlock(&m[command->session_id]);
    }
    /* CLOSE CLIENT PIPE (WILL ONLY REACH HERE AFTER SHUTDOWN */
    do
        {
            r = close(cpipe);
        } while (r == -1 && errno == EINTR);

        if (r == -1)
        {
            fprintf(stderr, "[ERR]: client pipe close by server failed: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }

    return NULL;
}