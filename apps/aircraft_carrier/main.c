#include <stdio.h>
#include <semaphore.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

#define START_LAND_TIME 100000

int read_args(int argc, char *argv[], int *n, int *k, int *planes_num);
void cleanup();
unsigned int random_utime(unsigned int min, unsigned int max);
void sigint_handler(int signum);
void *plane_thread(void *arg);
void thread_cleanup(void *args);
void start(int plane_id);
void land(int plane_id);
void free_airstrip();

int n, k, planes_num, on_aircraft_carrier = 0, available = 1;
pthread_mutex_t aircraft_carrier_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t start_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t land_cond = PTHREAD_COND_INITIALIZER;
int start_counter = 0, land_counter = 0;
pthread_t *threads_ids;

int main(int argc, char *argv[]) {
    atexit(cleanup);
    struct sigaction act;
    act.sa_handler = sigint_handler;
    sigaction(SIGINT, &act, NULL);
    sigaction(SIGTSTP, &act, NULL);

    char *args_help = "Enter N, K and a number of planes.\n";
    if (read_args(argc, argv, &n, &k, &planes_num) != 0) {
        printf(args_help);
        return 1;
    }

    threads_ids = malloc(planes_num * sizeof(pthread_t));
    if (threads_ids == NULL) {
        printf("Error while allocating memory occurred.\n");
        return 1;
    }
    // prepare mask for planes' threads (after that, only main thread will catch signals).
    sigset_t signal_mask;
    sigset_t old_signal_mask;
    sigfillset(&signal_mask);
    pthread_sigmask(SIG_SETMASK, &signal_mask, &old_signal_mask);
    for (int i = 0; i < planes_num; i++) {
        if (pthread_create(&(threads_ids[i]), NULL, plane_thread, NULL) != 0) {
            printf("Error while creating new thread occurred.\n");
            break;
        }
    }
    pthread_sigmask(SIG_SETMASK, &old_signal_mask, NULL);

    while (1)
        pause();
}

unsigned int random_utime(unsigned int min, unsigned int max) {
    return ((unsigned)rand() % (max - min)) + min;
}

void *plane_thread(void *arg) {
    pthread_cleanup_push(thread_cleanup, NULL);
    int plane_id = 0;
    for (int i = 0; i < planes_num; i++) {
        if (pthread_equal(threads_ids[i], pthread_self())) {
            plane_id = i;
            break;
        }
    }

    unsigned int min_time = 500000, max_time = 1000000;
    while (1) {
        usleep(random_utime(min_time, max_time));
        land(plane_id);
        usleep(random_utime(min_time, max_time));
        start(plane_id);
    }
    pthread_cleanup_pop(1);
    return NULL;
}

void free_airstrip() {
    if (on_aircraft_carrier < k)
        if (land_counter > 0)
            pthread_cond_signal(&land_cond);
        else
            pthread_cond_signal(&start_cond);
    else
        if (start_counter > 0)
            pthread_cond_signal(&start_cond);
        else
            pthread_cond_signal(&land_cond);
}

void start(int plane_id) {
    pthread_mutex_lock(&aircraft_carrier_mutex);
    printf("%3d | Plane #%-3d is going to start.\n", on_aircraft_carrier, plane_id);
    start_counter++;
    while (!available || (on_aircraft_carrier < k && land_counter > 0)) {
        pthread_cond_wait(&start_cond, &aircraft_carrier_mutex);
    }

    on_aircraft_carrier--;
    start_counter--;
    printf("%3d | Plane #%-3d is starting.\n", on_aircraft_carrier, plane_id);
    available = 0;
    pthread_mutex_unlock(&aircraft_carrier_mutex);

    usleep(START_LAND_TIME);

    pthread_mutex_lock(&aircraft_carrier_mutex);
    available = 1;
    free_airstrip();
    pthread_mutex_unlock(&aircraft_carrier_mutex);
}

void land(int plane_id) {
    pthread_mutex_lock(&aircraft_carrier_mutex);
    printf("%3d | Plane #%-3d is going to land.\n", on_aircraft_carrier, plane_id);
    land_counter++;
    while (!available || on_aircraft_carrier == n || (on_aircraft_carrier >= k && start_counter > 0)) {
        pthread_cond_wait(&land_cond, &aircraft_carrier_mutex);
    }

    on_aircraft_carrier++;
    land_counter--;
    printf("%3d | Plane #%-3d is landing.\n", on_aircraft_carrier, plane_id);
    available = 0;
    pthread_mutex_unlock(&aircraft_carrier_mutex);

    usleep(START_LAND_TIME);

    pthread_mutex_lock(&aircraft_carrier_mutex);
    available = 1;
    free_airstrip();
    pthread_mutex_unlock(&aircraft_carrier_mutex);
}

int read_args(int argc, char *argv[], int *n, int *k, int *planes_num) {
    if (argc != 4) {
        printf("Incorrect number of arguments.\n");
        return 1;
    }
    int arg_num = 1;
    *n = atoi(argv[arg_num++]);
    if (*n < 2) {
        printf("Incorrect N. It should be > 1.\n");
        return 1;
    }
    *k = atoi(argv[arg_num++]);
    if (*k < 1 || *k > *n - 1) {
        printf("Incorrect K. It should be > 0 and < N.\n");
        return 1;
    }
    *planes_num = atoi(argv[arg_num++]);
    if (*planes_num < 1) {
        printf("Incorrect number of planes. It should be > 0.\n");
        return 1;
    }

    return 0;
}

void thread_cleanup(void *args) {
    pthread_mutex_unlock(&aircraft_carrier_mutex);
}

void cleanup() {
    for (int i = 0; i < planes_num; i++)
        pthread_cancel(threads_ids[i]);
    for (int i = 0; i < planes_num; i++)
        pthread_join(threads_ids[i], NULL);
    pthread_cond_destroy(&start_cond);
    pthread_cond_destroy(&land_cond);
    pthread_mutex_destroy(&aircraft_carrier_mutex);
}

void sigint_handler(int signum) {
    printf("Program closed.\n");
    exit(0);
}
