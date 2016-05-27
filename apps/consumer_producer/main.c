#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include "main.h"

void sigint_handler(int signum);
void cleanup();
int read_args(int argc, char *argv[], int *producers_num, int *consumers_num);
char *get_app_path(char *app_name, char *main_path);

int sem_id;
int shm_id;
int producers_num, consumers_num;
pid_t *producers;
pid_t *consumers;

int main(int argc, char *argv[]) {
    atexit(cleanup);
    struct sigaction act;
    memset(&act, 0, sizeof act);
    act.sa_handler = sigint_handler;
    sigset_t full_mask;
    sigfillset(&full_mask);
    act.sa_mask = full_mask;
    sigaction(SIGINT, &act, NULL);
    sigaction(SIGTSTP, &act, NULL);

    char *args_help = "Enter number of producers and number of consumers.\n";
    if (read_args(argc, argv, &producers_num, &consumers_num) != 0) {
        printf(args_help);
        return 1;
    }

    shm_id = shmget(SHM_KEY, MEM_SIZE, IPC_CREAT | S_IWUSR | S_IRUSR);
    sem_id = semget(SEM_KEY, 4, IPC_CREAT | S_IWUSR | S_IRUSR);

    if (shm_id < 0 || sem_id < 0) {
        printf("Error while creating shared memory and/or semaphores occurred.\n");
        return 1;
    }

    union semun init_sem_val;
    init_sem_val.val = 0;
    semctl(sem_id, 0, SETVAL, init_sem_val);
    init_sem_val.val = ARRAY_LEN;
    semctl(sem_id, 1, SETVAL, init_sem_val);
    init_sem_val.val = 1;
    semctl(sem_id, 2, SETVAL, init_sem_val);
    semctl(sem_id, 3, SETVAL, init_sem_val);

    producers = malloc(producers_num * sizeof(pid_t));
    for (int i = 0; i < producers_num; i++)
        producers[i] = 0;
    consumers = malloc(consumers_num * sizeof(pid_t));
    for (int i = 0; i < consumers_num; i++)
        consumers[i] = 0;
    char * producer_exe = get_app_path("cp_producer", argv[0]);
    char * consumer_exe = get_app_path("cp_consumer", argv[0]);
    for (int i = 0; i < producers_num; i++) {
        pid_t pid = fork();
        if (pid < 0)
            printf("Error while creating new process occurred.\n");
        else if (pid == 0) {
            sigprocmask(SIG_SETMASK, &full_mask, NULL);
            execl(producer_exe, producer_exe, NULL);
        }
        else
            producers[i] = pid;
    }
    for (int i = 0; i < consumers_num; i++) {
        pid_t pid = fork();
        if (pid < 0)
            printf("Error while creating new process occurred.\n");
        else if (pid == 0) {
            sigprocmask(SIG_SETMASK, &full_mask, NULL);
            execl(consumer_exe, consumer_exe, NULL);
        }
        else
            consumers[i] = pid;
    }
    free(producer_exe);
    free(consumer_exe);

    while (1)
        pause();
}

int read_args(int argc, char *argv[], int *producers_num, int *consumers_num) {
    if (argc != 3) {
        printf("Incorrect number of arguments.\n");
        return 1;
    }
    int arg_num = 1;
    *producers_num = atoi(argv[arg_num++]);
    if (*producers_num < 1) {
        printf("Incorrect number of producers. It should be > 0.\n");
        return 1;
    }
    *consumers_num = atoi(argv[arg_num]);
    if (*consumers_num < 1) {
        printf("Incorrect  number of consumers. It should be > 0.\n");
        return 1;
    }

    return 0;
}

char *get_app_path(char *app_name, char *main_path) {
    char *path = malloc(strlen(main_path) + 50);
    if (path == NULL) {
        printf("Error while allocating memory occurred.\n");
        exit(0);
    }
    size_t end_of_path_index = strlen(main_path) - 1;
    while (main_path[end_of_path_index] != '/')
        end_of_path_index--;
    strncpy(path, main_path, end_of_path_index + 1);
    path[end_of_path_index + 1] = '\0';
    strcat(path, app_name);
    return path;
}

void cleanup() {
    for (int i = 0; i < producers_num; i++) {
        if (producers[i] != 0) {
            kill(producers[i], SIGUSR1);
            waitpid(producers[i], NULL, 0);
        }
    }
    for (int i = 0; i < consumers_num; i++) {
        if (consumers[i] != 0) {
            kill(consumers[i], SIGUSR1);
            waitpid(consumers[i], NULL, 0);
        }
    }

    free(producers);
    free(consumers);
    if (sem_id >= 0)
        semctl(sem_id, 0, IPC_RMID);
    if (shm_id >= 0)
        shmctl(shm_id, IPC_RMID, NULL);
}

void sigint_handler(int signum) {
    exit(0);
}