// mutex_better.c
#include <stdio.h>
#include <pthread.h>

#define NUM_THREADS      4
#define COUNT_PER_THREAD 100000

long counter = 0;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

void *worker(void *arg) {
    int id = *(int*)arg;

    /*
     * local_sum 是每個 Thread 自己的區域變數
     * 存在 Stack 上，不共享，完全不需要 lock
     */
    long local_sum = 0;

    for (int i = 0; i < COUNT_PER_THREAD; i++) {
        local_sum++;   // 不需要 lock，只有自己看得到
    }

    /*
     * 迴圈跑完，只需要 lock 一次
     * 把自己的部分結果加進共享的 counter
     *
     * 400000 次 lock/unlock → 4 次 lock/unlock
     */
    pthread_mutex_lock(&lock);
    counter += local_sum;
    pthread_mutex_unlock(&lock);

    printf("Thread %d 完成，local_sum = %ld\n", id, local_sum);
    return NULL;
}

int main() {
    pthread_t tids[NUM_THREADS];
    int ids[NUM_THREADS];

    for (int i = 0; i < NUM_THREADS; i++) {
        ids[i] = i;
        pthread_create(&tids[i], NULL, worker, &ids[i]);
    }

    for (int i = 0; i < NUM_THREADS; i++)
        pthread_join(tids[i], NULL);

    printf("\n最終 counter = %ld\n", counter);
    printf("預期結果     = %d\n", NUM_THREADS * COUNT_PER_THREAD);

    pthread_mutex_destroy(&lock);
    return 0;
}