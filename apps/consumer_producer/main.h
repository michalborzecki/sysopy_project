#ifndef ZAD2_MAIN_H
#define ZAD2_MAIN_H

#define SHM_KEY 12345
#define SEM_KEY 54321
#define ARRAY_LEN 50
#define MEM_SIZE (ARRAY_LEN + 2) * sizeof(int)

union semun {
    int val;
    unsigned short *array;
};

struct shm_mem {
    int start_index;
    int end_index;
    int tasks[ARRAY_LEN];
};

#endif //ZAD2_MAIN_H
