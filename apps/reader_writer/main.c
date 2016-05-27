#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <semaphore.h>
#include <sys/wait.h>
#include "main.h"

void sigint_handler(int signum);
void cleanup();
char *get_app_path(char *app_name, char *main_path);
int read_args(int argc, char *argv[], int *readers_num, int *writers_num);

sem_t * sem_id_w;
sem_t * sem_id_r;
int shm_id;
struct shm_mem * shm = (struct shm_mem *)-1;
int writers_num, readers_num;
pid_t *writers;
pid_t *readers;

int main(int argc, char *argv[]) {
    char *args_help = "Enter number of readers and number of writers.\n";
    if (read_args(argc, argv, &readers_num, &writers_num) != 0) {
        printf(args_help);
        return 1;
    }

    atexit(cleanup);
    struct sigaction act;
    memset(&act, 0, sizeof act);
    act.sa_handler = sigint_handler;
    sigset_t full_mask;
    sigfillset(&full_mask);
    act.sa_mask = full_mask;
    sigaction(SIGINT, &act, NULL);
    sigaction(SIGTSTP, &act, NULL);

    shm_id = shm_open(SHM_NAME, O_CREAT | O_RDWR, S_IWUSR | S_IRUSR);
    if (shm_id < 0 || ftruncate(shm_id, MEM_SIZE) < 0 ||
            (shm = (struct shm_mem *)mmap(0, MEM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_id, 0)) < 0) {
        printf("Error while creating shared memory occurred.\n");
        return 1;
    }
    for (int i = 0; i < ARRAY_LEN; i++) {
        shm->numbers[i] = 0;
    }
    munmap(shm, MEM_SIZE);
    sem_id_w = sem_open(SEM_NAME_W, O_CREAT, S_IWUSR | S_IRUSR, 1);
    sem_id_r = sem_open(SEM_NAME_R, O_CREAT, S_IWUSR | S_IRUSR, MAX_READERS);
    if (sem_id_w < 0 || sem_id_r < 0) {
        printf("Error while creating semaphores occurred.\n");
        return 1;
    }

    writers = malloc(writers_num * sizeof(pid_t));
    for (int i = 0; i < writers_num; i++)
        writers[i] = 0;
    readers = malloc(readers_num * sizeof(pid_t));
    for (int i = 0; i < readers_num; i++)
        readers[i] = 0;
    char * writer_exe = get_app_path("rw_writer", argv[0]);
    char * reader_exe = get_app_path("rw_reader", argv[0]);
    for (int i = 0; i < writers_num; i++) {
        pid_t pid = fork();
        if (pid < 0)
            printf("Error while creating new process occurred.\n");
        else if (pid == 0) {
            sigprocmask(SIG_SETMASK, &full_mask, NULL);
            execl(writer_exe, writer_exe, NULL);
        }
        else
            writers[i] = pid;
    }
    for (int i = 0; i < readers_num; i++) {
        pid_t pid = fork();
        if (pid < 0)
            printf("Error while creating new process occurred.\n");
        else if (pid == 0) {
            sigprocmask(SIG_SETMASK, &full_mask, NULL);
            execl(reader_exe, reader_exe, NULL);
        }
        else
            readers[i] = pid;
    }
    free(writer_exe);
    free(reader_exe);

    while (1)
        pause();
}

int read_args(int argc, char *argv[], int *readers_num, int *writers_num) {
    if (argc != 3) {
        printf("Incorrect number of arguments.\n");
        return 1;
    }
    int arg_num = 1;
    *readers_num = atoi(argv[arg_num++]);
    if (*readers_num < 1) {
        printf("Incorrect number of readers. It should be > 0.\n");
        return 1;
    }
    *writers_num = atoi(argv[arg_num]);
    if (*writers_num < 1) {
        printf("Incorrect number of writers. It should be > 0.\n");
        return 1;
    }

    if (*readers_num > MAX_READERS) {
        printf("Readers numbers must be <= %d.\n", MAX_READERS);
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
    for (int i = 0; i < writers_num; i++) {
        if (writers[i] != 0) {
            kill(writers[i], SIGUSR1);
            waitpid(writers[i], NULL, 0);
        }
    }
    for (int i = 0; i < readers_num; i++) {
        if (readers[i] != 0) {
            kill(readers[i], SIGUSR1);
            waitpid(readers[i], NULL, 0);
        }
    }

    free(writers);
    free(readers);
    if (sem_id_w >= 0) {
        sem_close(sem_id_w);
        sem_unlink(SEM_NAME_W);
    }
    if (sem_id_r >= 0) {
        sem_close(sem_id_r);
        sem_unlink(SEM_NAME_R);
    }
    if (shm_id >= 0) {
        close(shm_id);
        shm_unlink(SHM_NAME);
    }
}

void sigint_handler(int signum) {
    exit(0);
}