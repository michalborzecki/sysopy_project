#include <stdio.h>
#include <semaphore.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>


int read_args(int argc, char *argv[], int *printers_num, int *processes_num);
void cleanup();
unsigned int random_utime(unsigned int min, unsigned int max);
void sigint_handler(int signum);
int reserve_printer(int id);
void release_printer(int id, int printer_no);
void *process_thread(void *arg);
void thread_cleanup(void *args);

int printers_num, processes_num;
int printers_available;
pthread_mutex_t reserve_printer_mutex;
pthread_cond_t reserve_printer_cond;

pthread_key_t reservation_locked_key;
pthread_t *threads_ids;
int *printers;

int main(int argc, char *argv[]) {
    atexit(cleanup);
    struct sigaction act;
    memset(&act, 0, sizeof act);
    act.sa_handler = sigint_handler;
    sigaction(SIGINT, &act, NULL);
    sigaction(SIGTSTP, &act, NULL);

    char *args_help = "Enter number of printers and number of processes.\n";
    if (read_args(argc, argv, &printers_num, &processes_num) != 0) {
        printf(args_help);
        return 1;
    }

    threads_ids = malloc(processes_num * sizeof(pthread_t));
    printers = malloc(printers_num * sizeof(int));
    if (threads_ids == NULL || printers == NULL) {
        printf("Error while allocating memory occurred.\n");
        return 1;
    }
    for (int i = 0; i < printers_num; i++)
        printers[i] = -1;
    printers_available = printers_num;
    pthread_mutex_init(&reserve_printer_mutex, NULL);
    pthread_cond_init(&reserve_printer_cond, NULL);
    pthread_key_create(&reservation_locked_key, NULL);
    // prepare mask for processes' threads (after that, only main thread will catch signals).
    sigset_t signal_mask;
    sigset_t old_signal_mask;
    sigfillset(&signal_mask);
    pthread_sigmask(SIG_SETMASK, &signal_mask, &old_signal_mask);
    for (int i = 0; i < processes_num; i++) {
        if (pthread_create(&(threads_ids[i]), NULL, process_thread, NULL) != 0) {
            printf("Error while creating new thread occurred.\n");
            break;
        }
    }
    pthread_sigmask(SIG_SETMASK, &old_signal_mask, NULL);

    while (1)
        pause();
}

void *process_thread(void *arg) {
    pthread_cleanup_push(thread_cleanup, NULL);
            int process_id = 0;
            for (int i = 0; i < processes_num; i++) {
                if (pthread_equal(threads_ids[i], pthread_self())) {
                    process_id = i;
                    break;
                }
            }
            int * reservation_locked = malloc(sizeof(int));
            *reservation_locked = 0;
            pthread_setspecific(reservation_locked_key, reservation_locked);
            unsigned int min_time = 500000, max_time = 1000000;
            int printer_no;
            while (1) {
                usleep(random_utime(min_time, max_time));
                printer_no = reserve_printer(process_id);
                usleep(random_utime(min_time, max_time));
                release_printer(process_id, printer_no);
            }
    pthread_cleanup_pop(1);
    return NULL;
}

int reserve_printer(int id) {
    int *reservation_locked = pthread_getspecific(reservation_locked_key);
    pthread_mutex_lock(&reserve_printer_mutex);
    *reservation_locked = 1;
    printf("%d is waiting for printer.\n", id);
    while (printers_available == 0) {
        pthread_cond_wait(&reserve_printer_cond, &reserve_printer_mutex);
    }
    int printer_no = 0;
    for (int i = 0; i < printers_num; i++) {
        if (printers[i] == -1) {
            printer_no = i;
            break;
        }
    }
    printers[printer_no] = id;
    printers_available--;
    printf("%d is using %d printer.\n", id, printer_no);
    pthread_mutex_unlock(&reserve_printer_mutex);
    *reservation_locked = 0;
    return printer_no;
}

void release_printer(int id, int printer_id) {
    int *reservation_locked = pthread_getspecific(reservation_locked_key);
    pthread_mutex_lock(&reserve_printer_mutex);
    *reservation_locked = 1;
    printers[printer_id] = -1;
    printers_available++;
    printf("%d released %d printer.\n", id, printer_id);
    fflush(stdout);
    pthread_cond_signal(&reserve_printer_cond);
    pthread_mutex_unlock(&reserve_printer_mutex);
    *reservation_locked = 0;
}

unsigned int random_utime(unsigned int min, unsigned int max) {
    return ((unsigned)rand() % (max - min)) + min;
}

int read_args(int argc, char *argv[], int *printers_num, int *processes_num) {
    if (argc != 3) {
        printf("Incorrect number of arguments.\n");
        return 1;
    }
    int arg_num = 1;
    *printers_num = atoi(argv[arg_num++]);
    if (*printers_num < 1) {
        printf("Incorrect number of printers. It should be > 0.\n");
        return 1;
    }
    *processes_num = atoi(argv[arg_num++]);
    if (*processes_num < 1) {
        printf("Incorrect number of processes. It should be > 0.\n");
        return 1;
    }

    return 0;
}

void thread_cleanup(void *args) {
    if (*(int *)pthread_getspecific(reservation_locked_key) == 1)
        pthread_mutex_unlock(&reserve_printer_mutex);
    free(pthread_getspecific(reservation_locked_key));
}

void cleanup() {
    for (int i = 0; i < processes_num; i++)
        pthread_cancel(threads_ids[i]);
    for (int i = 0; i < processes_num; i++)
        pthread_join(threads_ids[i], NULL);
    pthread_cond_destroy(&reserve_printer_cond);
    pthread_mutex_destroy(&reserve_printer_mutex);
    pthread_key_delete(reservation_locked_key);
    free(threads_ids);
}

void sigint_handler(int signum) {
    printf("Program closed.\n");
    exit(0);
}
