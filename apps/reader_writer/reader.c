#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <semaphore.h>
#include <string.h>
#include <sys/time.h>
#include <string.h>
#include "main.h"

void sigint_handler(int signum);
void cleanup();

sem_t * sem_id_r;
int shm_id;
struct shm_mem * shm = (struct shm_mem *)-1;
struct timespec delay = {0, 100000000l};

int main(int argc, char *argv[]) {
    atexit(cleanup);
    sigset_t mask;
    sigfillset(&mask);
    sigdelset(&mask, SIGUSR1);
    sigprocmask(SIG_SETMASK, &mask, NULL);
    struct sigaction act;
    memset(&act, 0, sizeof act);
    act.sa_handler = sigint_handler;
    sigaction(SIGUSR1, &act, NULL);

    shm_id = shm_open(SHM_NAME, O_RDWR, 0);
    if (shm_id < 0 || (shm = (struct shm_mem *)mmap(0, MEM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_id, 0)) < 0) {
        printf("Error while opening shared memory occurred.\n");
        return 1;
    }

    sem_id_r = sem_open(SEM_NAME_R, 0);
    if (sem_id_r < 0) {
        printf("Error while opening semaphores occurred.\n");
        return 1;
    }

    while (1) {
        if (sem_wait(sem_id_r) < 0) {
            printf("Error while waiting for semaphore occurred.\n");
            return 1;
        }
        printf("%d is reading.\n", getpid());
        fflush(stdout);
        printf("%d has stopped reading.\n", getpid());
        fflush(stdout);
        if (sem_post(sem_id_r) < 0) {
            printf("Error while incrementing semaphore occurred.\n");
            return 1;
        }
        nanosleep(&delay, NULL); // some important calculations here
    }
}

void cleanup() {
    if (sem_id_r >= 0) {
        sem_close(sem_id_r);
    }
    if (shm_id >= 0) {
        if (shm >= 0) {
            munmap(shm, MEM_SIZE);
        }
        close(shm_id);
    }
}

void sigint_handler(int signum) {
    exit(0);
}