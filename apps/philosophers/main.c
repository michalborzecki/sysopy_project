#include <stdio.h>
#include <semaphore.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

void cleanup();
void sigint_handler(int signum);
void *philosopher_thread(void *arg);
void thread_cleanup(void *args);

sem_t forks[5];
sem_t waiter;
pthread_t threads_ids[5];
static pthread_mutex_t printf_fork_mutex[5];
pthread_key_t printf_left_fork_locked;
pthread_key_t printf_right_fork_locked;

int main(int argc, char *argv[]) {
    atexit(cleanup);
    struct sigaction act;
    act.sa_handler = sigint_handler;
    sigaction(SIGINT, &act, NULL);
    sigaction(SIGTSTP, &act, NULL);
    for (int i = 0; i < 5; i++) {
        if (sem_init(&(forks[i]), 0, 1) != 0) {
            printf("Error while creating semaphore occurred.\n");
            return 1;
        }
    }
    if (sem_init(&waiter, 0, 4) != 0) {
        printf("Error while creating semaphore occurred.\n");
        return 1;
    }
    for (int i = 0; i < 5; i++)
        pthread_mutex_init(&printf_fork_mutex[i], NULL);

    pthread_key_create(&printf_left_fork_locked, NULL);
    pthread_key_create(&printf_right_fork_locked, NULL);
    // prepare mask for philosophers' threads (after that, only main thread will catch signals).
    sigset_t signal_mask;
    sigset_t old_signal_mask;
    sigfillset(&signal_mask);
    pthread_sigmask(SIG_SETMASK, &signal_mask, &old_signal_mask);
    for (int i = 0; i < 5; i++) {
        if (pthread_create(&(threads_ids[i]), NULL, philosopher_thread, NULL) != 0) {
            printf("Error while creating new thread occurred.\n");
            break;
        }
    }
    pthread_sigmask(SIG_SETMASK, &old_signal_mask, NULL);

    while (1)
        pause();
}

void *philosopher_thread(void *arg) {
    pthread_cleanup_push(thread_cleanup, NULL);
    int philosopher_id = 0;
    for (int i = 0; i < 5; i++) {
        if (pthread_equal(threads_ids[i], pthread_self())) {
            philosopher_id = i;
            break;
        }
    }
    unsigned int thinking_utime = 0;
    unsigned int eating_time = 500000;
    int left_fork = philosopher_id, right_fork = (philosopher_id + 1) % 5;
    int * left_fork_locked = malloc(sizeof(int));
    int * right_fork_locked = malloc(sizeof(int));
    *left_fork_locked = *right_fork_locked = 0;
    pthread_setspecific(printf_left_fork_locked, left_fork_locked);
    pthread_setspecific(printf_right_fork_locked, right_fork_locked);
    while (1) {
        printf("Philosopher #%d is thinking.\n", philosopher_id);
        thinking_utime = ((unsigned)rand() % 500000) + 500000;
        usleep(thinking_utime);

        printf("Philosopher #%d is going to eat.\n", philosopher_id);
        sem_wait(&waiter);

        printf("Philosopher #%d wants to take #%d fork.\n", philosopher_id, left_fork);
        fflush(stdout);
        sem_wait(&forks[left_fork]);
        pthread_mutex_lock(&printf_fork_mutex[left_fork]);
        *left_fork_locked = 1;
        printf("Philosopher #%d has taken #%d fork.\n", philosopher_id, left_fork);
        fflush(stdout);
        pthread_mutex_unlock(&printf_fork_mutex[left_fork]);
        *left_fork_locked = 0;

        printf("Philosopher #%d wants to take #%d fork.\n", philosopher_id, right_fork);
        fflush(stdout);
        sem_wait(&forks[right_fork]);
        pthread_mutex_lock(&printf_fork_mutex[right_fork]);
        *right_fork_locked = 1;
        printf("Philosopher #%d has taken #%d fork.\n", philosopher_id, right_fork);
        pthread_mutex_unlock(&printf_fork_mutex[right_fork]);
        *right_fork_locked = 0;

        printf("Philosopher #%d is eating.\n", philosopher_id);
        fflush(stdout);
        usleep(eating_time);

        pthread_mutex_lock(&printf_fork_mutex[left_fork]);
        *left_fork_locked = 1;
        sem_post(&forks[left_fork]);
        printf("Philosopher #%d has put #%d fork.\n", philosopher_id, left_fork);
        fflush(stdout);
        pthread_mutex_unlock(&printf_fork_mutex[left_fork]);
        *left_fork_locked = 0;

        pthread_mutex_lock(&printf_fork_mutex[right_fork]);
        *right_fork_locked = 1;
        sem_post(&forks[right_fork]);
        printf("Philosopher #%d has put #%d fork.\n", philosopher_id, right_fork);
        fflush(stdout);
        pthread_mutex_unlock(&printf_fork_mutex[right_fork]);
        *right_fork_locked = 0;

        sem_post(&waiter);
    }
    pthread_cleanup_pop(1);
    return NULL;
}

void cleanup() {
    for (int i = 0; i < 5; i++)
        pthread_cancel(threads_ids[i]);
    for (int i = 0; i < 5; i++)
        pthread_join(threads_ids[i], NULL);
    for (int i = 0; i < 5; i++) {
        pthread_mutex_destroy(&printf_fork_mutex[i]);
        sem_destroy(&forks[i]);
    }
    sem_destroy(&waiter);
    pthread_key_delete(printf_left_fork_locked);
    pthread_key_delete(printf_right_fork_locked);
}

void thread_cleanup(void *args) {
    int philosopher_id = 0;
    for (int i = 0; i < 5; i++) {
        if (pthread_equal(threads_ids[i], pthread_self())) {
            philosopher_id = i;
            break;
        }
    }
    if (*(int *)pthread_getspecific(printf_left_fork_locked) == 1)
        pthread_mutex_unlock(&printf_fork_mutex[philosopher_id]);
    if (*(int *)pthread_getspecific(printf_right_fork_locked) == 1)
        pthread_mutex_unlock(&printf_fork_mutex[(philosopher_id + 1) % 5]);
    free(pthread_getspecific(printf_left_fork_locked));
    free(pthread_getspecific(printf_right_fork_locked));
}

void sigint_handler(int signum) {
    printf("Program closed.\n");
    exit(0);
}
