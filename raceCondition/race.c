// race.c
#include <stdio.h>
#include <pthread.h>

#define NUM_THREADS   4
#define COUNT_PER_THREAD 100000

/*
 * 共享的全域變數
 * 四個 Thread 都會對它加一
 * 預期結果：4 * 100000 = 400000
 */
long counter = 0;

/*
 * Mutex 宣告和初始化
 *
 * PTHREAD_MUTEX_INITIALIZER 是靜態初始化的寫法
 * 適合全域變數，不需要另外呼叫 pthread_mutex_init()
 *
 * 等價的動態初始化寫法：
 *   pthread_mutex_t lock;
 *   pthread_mutex_init(&lock, NULL);  ← 通常在 main() 裡呼叫
 */
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

void *worker(void *arg) {
    int id = *(int*)arg;

    for (int i = 0; i < COUNT_PER_THREAD; i++) {
        /*
         * lock：進入 Critical Section
         *
         * 如果 lock 已經被別的 Thread 持有
         * 這個 Thread 就在這裡睡眠等待
         * 直到持有者呼叫 unlock 才被喚醒
         */
        pthread_mutex_lock(&lock);
        
        /*
         * 這行看起來是一個操作
         * 但 CPU 實際上做三步：
         *   1. 從記憶體讀 counter 的值到暫存器
         *   2. 暫存器的值加一
         *   3. 把暫存器的值寫回記憶體
         *
         * 這三步之間，OS 隨時可能切換到另一個 Thread
         * 就會出現 Race Condition
         */
        counter++;

        /*
         * unlock：離開 Critical Section
         *
         * 釋放 lock，讓等待的 Thread 可以進入
         * 注意：lock 和 unlock 必須成對出現
         * 如果忘記 unlock，其他 Thread 永遠進不來（Deadlock）
         */
        pthread_mutex_unlock(&lock);
    }

    printf("Thread %d 完成\n", id);
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
    printf("差距         = %ld\n",
           (long)(NUM_THREADS * COUNT_PER_THREAD) - counter);

    /*
     * 銷毀 Mutex，釋放資源
     * 靜態初始化的也要銷毀
     */
    pthread_mutex_destroy(&lock);

    return 0;
}