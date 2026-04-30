/*
一個大小為 N 的 buffer (circular queue -> 讓 in / out 指標繞著走)
Producer Thread: 產生資料放進 buffer
Consumer Thread: 從 buffer 取出資料處理他

需要保證：
    buffer 滿了 -> Producer 等待
    buffer 空了 -> Consumer 等待    
    同時間只有一個 Thread 在操作 buffer

mutex = 1 : 保護 buffer 的互斥存取，任何時候只有一個 Thread 讀寫 buffer
empty = N : 目前有幾個空位能放資料，Producer 放之前先 wait(empty)，放完 signal(full)
full = 0 : 目前有幾個資料能取，Consumer 取之前先 wait(full)，取完 signal(empty)
*/
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>

#define BUFFER_SIZE    5
#define NUM_PRODUCERS  2
#define NUM_CONSUMERS  3
#define ITEMS_PER_PRODUCER 8   // 每個 Producer 產生幾個資料

// ── 共享的 Buffer ──────────────────────────────────────

int buffer[BUFFER_SIZE];
int in  = 0;   // Producer 下一個放入的位置
int out = 0;   // Consumer 下一個取出的位置

// ── 三個 Semaphore ─────────────────────────────────────

sem_t mutex;   // 互斥鎖（初始值 1）
sem_t empty;   // 空槽數量（初始值 BUFFER_SIZE）
sem_t full;    // 有資料的槽數量（初始值 0）

// ── 用來追蹤總共消費了幾個 ─────────────────────────────

int total_consumed = 0;
int total_expected = NUM_PRODUCERS * ITEMS_PER_PRODUCER;

// ── Producer ───────────────────────────────────────────

void *producer(void *arg) {
    int id = *(int*)arg;

    for (int i = 0; i < ITEMS_PER_PRODUCER; i++) {

        // 產生一個資料（這裡用簡單的數字表示）
        int item = id * 100 + i;

        /*
         * 步驟一：確認有空槽可以放
         *
         * sem_wait 把 empty 的值減一
         * 如果 empty 已經是 0（buffer 滿了），在這裡睡眠等待
         * 直到有 Consumer 取走資料並 sem_post(empty) 才被喚醒
         */
        sem_wait(&empty);

        /*
         * 步驟二：進入 Critical Section
         *
         * 確保同一時間只有一個 Thread 在操作 buffer
         */
        sem_wait(&mutex);

        // ── Critical Section 開始 ──
        buffer[in] = item;
        in = (in + 1) % BUFFER_SIZE;   // 移動到下一個位置（環形）

        // 計算目前 buffer 使用量（用來顯示）
        int used = 0;
        int tmp_in = in, tmp_out = out;
        if (tmp_in >= tmp_out)
            used = tmp_in - tmp_out;
        else
            used = BUFFER_SIZE - tmp_out + tmp_in;

        printf("Producer %d 放入: %3d  [buffer: %d/%d]\n",
               id, item, used, BUFFER_SIZE);
        // ── Critical Section 結束 ──

        sem_post(&mutex);   // 釋放互斥鎖
        sem_post(&full);    // 通知 Consumer 又多了一個資料

        /*
         * 模擬生產耗時
         * usleep 單位是微秒（1秒 = 1000000微秒）
         * 這裡是 0.1 ~ 0.3 秒
         */
        usleep((rand() % 3 + 1) * 100000);
    }

    printf("Producer %d 完成生產\n", id);
    return NULL;
}

// ── Consumer ───────────────────────────────────────────

void *consumer(void *arg) {
    int id = *(int*)arg;

    while (1) {
        /*
         * 步驟一：確認有資料可以取
         *
         * sem_wait 把 full 的值減一
         * 如果 full 已經是 0（buffer 空了），在這裡睡眠等待
         * 直到有 Producer 放入資料並 sem_post(full) 才被喚醒
         */
        sem_wait(&full);

        // 步驟二：進入 Critical Section
        sem_wait(&mutex);

        // ── Critical Section 開始 ──
        int item = buffer[out];
        out = (out + 1) % BUFFER_SIZE;
        total_consumed++;
        int done = (total_consumed >= total_expected);
        // ── Critical Section 結束 ──

        sem_post(&mutex);    // 釋放互斥鎖
        sem_post(&empty);    // 通知 Producer 又多了一個空槽

        printf("Consumer %d 取出: %3d\n", id, item);

        // 模擬消費耗時
        usleep((rand() % 5 + 1) * 100000);

        /*
         * 所有資料都消費完了就結束
         * done 在 Critical Section 裡讀取，這裡只是用來判斷
         */
        if (done) break;
    }

    printf("Consumer %d 結束\n", id);
    return NULL;
}

// ── Main ───────────────────────────────────────────────

int main() {
    pthread_t prod_tids[NUM_PRODUCERS];
    pthread_t cons_tids[NUM_CONSUMERS];
    int prod_ids[NUM_PRODUCERS];
    int cons_ids[NUM_CONSUMERS];

    /*
     * 初始化三個 Semaphore
     *
     * sem_init 參數：
     *   第一個：Semaphore 的指標
     *   第二個：0 = 同一個 Process 的 Thread 之間共用
     *           1 = 不同 Process 之間共用（需要 shared memory）
     *   第三個：初始值
     */
    sem_init(&mutex, 0, 1);            // 互斥鎖，初始可進入
    sem_init(&empty, 0, BUFFER_SIZE);  // 一開始全部都是空槽
    sem_init(&full,  0, 0);            // 一開始沒有資料

    printf("Buffer 大小：%d\n", BUFFER_SIZE);
    printf("Producer 數量：%d，每個生產 %d 個\n",
           NUM_PRODUCERS, ITEMS_PER_PRODUCER);
    printf("Consumer 數量：%d\n", NUM_CONSUMERS);
    printf("總共需要消費：%d 個\n\n", total_expected);

    // 建立 Consumer（先建，讓它們準備好等資料）
    for (int i = 0; i < NUM_CONSUMERS; i++) {
        cons_ids[i] = i;
        pthread_create(&cons_tids[i], NULL, consumer, &cons_ids[i]);
    }

    // 建立 Producer
    for (int i = 0; i < NUM_PRODUCERS; i++) {
        prod_ids[i] = i;
        pthread_create(&prod_tids[i], NULL, producer, &prod_ids[i]);
    }

    // 等待所有 Producer 完成
    for (int i = 0; i < NUM_PRODUCERS; i++)
        pthread_join(prod_tids[i], NULL);

    // 等待所有 Consumer 完成
    for (int i = 0; i < NUM_CONSUMERS; i++)
        pthread_join(cons_tids[i], NULL);

    printf("\n所有工作完成，總共消費了 %d 個資料\n", total_consumed);

    // 清理 Semaphore
    sem_destroy(&mutex);
    sem_destroy(&empty);
    sem_destroy(&full);

    return 0;
}