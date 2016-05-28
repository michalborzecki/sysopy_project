#include <stdio.h>
#include <semaphore.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>


int read_args(int argc, char *argv[], int *pairs_num);
void cleanup();
unsigned int random_utime(unsigned int min, unsigned int max);
void sigint_handler(int signum);
void get_table(int id);
void release_table(int id);
void *person_thread(void *arg);
void thread_cleanup(void *args);

int pairs_num, using_table = 0;
int *waiting_for_pair;
pthread_mutex_t *waiting_for_pair_mutex;
pthread_cond_t *waiting_for_pair_cond;
pthread_mutex_t waiting_for_table_mutex;
pthread_cond_t waiting_for_table_cond;

pthread_key_t table_locked_key;
pthread_key_t pair_locked_key;
pthread_t *threads_ids;

int main(int argc, char *argv[]) {
    atexit(cleanup);
    struct sigaction act;
    memset(&act, 0, sizeof act);
    act.sa_handler = sigint_handler;
    sigaction(SIGINT, &act, NULL);
    sigaction(SIGTSTP, &act, NULL);

    char *args_help = "Enter number of pairs.\n";
    if (read_args(argc, argv, &pairs_num) != 0) {
        printf(args_help);
        return 1;
    }

    threads_ids = malloc(pairs_num * 2 * sizeof(pthread_t));
    waiting_for_pair = malloc(pairs_num * sizeof(int));
    waiting_for_pair_cond = malloc(pairs_num * sizeof(pthread_cond_t));
    waiting_for_pair_mutex = malloc(pairs_num * sizeof(pthread_mutex_t));
    if (threads_ids == NULL || waiting_for_pair_cond == NULL || waiting_for_pair_mutex == NULL) {
        printf("Error while allocating memory occurred.\n");
        return 1;
    }
    for (int i = 0; i < pairs_num; i++) {
        waiting_for_pair[i] = 0;
        pthread_mutex_init(&waiting_for_pair_mutex[i], NULL);
        pthread_cond_init(&waiting_for_pair_cond[i], NULL);
    }
    pthread_mutex_init(&waiting_for_table_mutex, NULL);
    pthread_cond_init(&waiting_for_table_cond, NULL);
    pthread_key_create(&table_locked_key, NULL);
    pthread_key_create(&pair_locked_key, NULL);
    // prepare mask for persons' threads (after that, only main thread will catch signals).
    sigset_t signal_mask;
    sigset_t old_signal_mask;
    sigfillset(&signal_mask);
    pthread_sigmask(SIG_SETMASK, &signal_mask, &old_signal_mask);
    for (int i = 0; i < pairs_num * 2; i++) {
        if (pthread_create(&(threads_ids[i]), NULL, person_thread, NULL) != 0) {
            printf("Error while creating new thread occurred.\n");
            break;
        }
    }
    pthread_sigmask(SIG_SETMASK, &old_signal_mask, NULL);

    while (1)
        pause();
}

void *person_thread(void *arg) {
    pthread_cleanup_push(thread_cleanup, NULL);
            int person_id = 0;
            for (int i = 0; i < pairs_num * 2; i++) {
                if (pthread_equal(threads_ids[i], pthread_self())) {
                    person_id = i;
                    break;
                }
            }
            int * table_locked = malloc(sizeof(int));
            int * pair_locked = malloc(sizeof(int));
            *table_locked = 0;
            *pair_locked = 0;
            pthread_setspecific(table_locked_key, table_locked);
            pthread_setspecific(pair_locked_key, pair_locked);
            unsigned int min_time = 500000, max_time = 1000000;
            while (1) {
                usleep(random_utime(min_time, max_time));
                get_table(person_id);
                usleep(random_utime(min_time, max_time));
                release_table(person_id);
            }
    pthread_cleanup_pop(1);
    return NULL;
}

void get_table(int id) {
    int abs_id = id % pairs_num;
    int is_first_in_pair = 0;
    int *table_locked = pthread_getspecific(table_locked_key);
    int *pair_locked = pthread_getspecific(pair_locked_key);
    pthread_mutex_lock(&waiting_for_pair_mutex[abs_id]);
    *pair_locked = 1;
    printf("%d is waiting for the second person.\n", id);
    while (waiting_for_pair[abs_id] == 0) {
        is_first_in_pair = 1;
        waiting_for_pair[abs_id] = 1;
        pthread_cond_wait(&waiting_for_pair_cond[abs_id], &waiting_for_pair_mutex[abs_id]);
    }
    if (!is_first_in_pair) {
        pthread_mutex_lock(&waiting_for_table_mutex);
        *table_locked = 1;
        printf("Pair %d and %d is complete. Waiting for table.\n", abs_id, abs_id + pairs_num);
        while (using_table > 0) {
            pthread_cond_wait(&waiting_for_table_cond, &waiting_for_table_mutex);
        }
        using_table = 2;
        printf("Pair %d and %d got a table.\n", abs_id, abs_id + pairs_num);
        pthread_cond_signal(&waiting_for_pair_cond[abs_id]);
        pthread_mutex_unlock(&waiting_for_table_mutex);
        *table_locked = 0;
    }
    else {
        waiting_for_pair[abs_id] = 0;
    }
    pthread_mutex_unlock(&waiting_for_pair_mutex[abs_id]);
    *pair_locked = 0;
}

void release_table(int id) {
    int *table_locked = pthread_getspecific(table_locked_key);
    pthread_mutex_lock(&waiting_for_table_mutex);
    *table_locked = 1;
    using_table--;
    printf("%d released the table.\n", id);
    if (using_table == 0) {
        pthread_cond_signal(&waiting_for_table_cond);
    }
    pthread_mutex_unlock(&waiting_for_table_mutex);
    *table_locked = 0;
}

unsigned int random_utime(unsigned int min, unsigned int max) {
    return ((unsigned)rand() % (max - min)) + min;
}

int read_args(int argc, char *argv[], int *pairs_num) {
    if (argc != 2) {
        printf("Incorrect number of arguments.\n");
        return 1;
    }
    int arg_num = 1;
    *pairs_num = atoi(argv[arg_num++]);
    if (*pairs_num < 1) {
        printf("Incorrect number of pairs. It should be > 0.\n");
        return 1;
    }

    return 0;
}

void thread_cleanup(void *args) {
    int person_id = 0;
    for (int i = 0; i < pairs_num * 2; i++) {
        if (pthread_equal(threads_ids[i], pthread_self())) {
            person_id = i;
            break;
        }
    }
    if (*(int *)pthread_getspecific(table_locked_key) == 1)
        pthread_mutex_unlock(&waiting_for_table_mutex);
    if (*(int *)pthread_getspecific(pair_locked_key) == 1)
        pthread_mutex_unlock(&waiting_for_pair_mutex[person_id % pairs_num]);
    free(pthread_getspecific(pair_locked_key));
    free(pthread_getspecific(table_locked_key));
}

void cleanup() {
    for (int i = 0; i < pairs_num * 2; i++)
        pthread_cancel(threads_ids[i]);
    for (int i = 0; i < pairs_num * 2; i++)
        pthread_join(threads_ids[i], NULL);
    for (int i = 0; i < pairs_num; i++) {
        pthread_cond_destroy(&waiting_for_pair_cond[i]);
        pthread_mutex_destroy(&waiting_for_pair_mutex[i]);
    }
    pthread_cond_destroy(&waiting_for_table_cond);
    pthread_mutex_destroy(&waiting_for_table_mutex);
    pthread_key_delete(table_locked_key);
    pthread_key_delete(pair_locked_key);
    free(threads_ids);
    free(waiting_for_pair_cond);
    free(waiting_for_pair_mutex);
}

void sigint_handler(int signum) {
    printf("Program closed.\n");
    exit(0);
}
