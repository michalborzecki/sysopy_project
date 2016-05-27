#ifndef ZAD2_MAIN_H
#define ZAD2_MAIN_H

#define SHM_NAME "/readerwritermem"
#define SEM_NAME_W "/writersem"
#define SEM_NAME_R "/readersem"
#define ARRAY_LEN 500
#define MEM_SIZE (ARRAY_LEN) * sizeof(int)
#define MAX_READERS 50

struct shm_mem {
    int numbers[ARRAY_LEN];
};

#endif //ZAD2_MAIN_H
