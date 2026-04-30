// 用 valgrind 的 helgrind 模式來檢測 race condition
// valgrind --tool=helgrind ./xxx
// 編譯時加上 -g 以便獲得更詳細的 debug 資訊 -> gcc -Wall -g pthread -o xxx xxx.c

// 開發 multi-threading 程式的標準流程：
// 1. coding
// 2. 編譯加上 -g
// 3. 用 helgrind 找 race condition 和 lock 順序問題
// 4. 用 memcheck 找記憶體問題 (memory leak , invalid read/write)
// 5. 修正問題後重複 2~4，直到沒有問題為止

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#define NUM_THREADS 4

/*
 * 一個簡單的統計結構
 * 記錄所有 Thread 的工作結果
 */
typedef struct {
    long total;       // 所有 Thread 的總和
    int  count;       // 完成的 Thread 數量
    long max_value;   // 所有 Thread 中最大的值
} Stats;

Stats stats = {0, 0, 0};

/*
 * 保護 total 和 count 的 mutex
 * 但故意沒有保護 max_value
 */
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

void *worker(void *arg) {
    int id = *(int*)arg;

    // 每個 Thread 計算自己的部分結果
    long local_result = 0;
    for (int i = 0; i < 100000; i++)
        local_result += (id + 1) * i;

    /*
     * 把結果合併進 stats
     */
    pthread_mutex_lock(&lock);
    stats.total += local_result;
    stats.count++;
    // if(local_result > stats.max_value) {
    //     stats.max_value = local_result;
    // }
    pthread_mutex_unlock(&lock);

    /*
     * 更新最大值
     *
     * 這裡故意沒有加 lock
     * 乍看之下好像沒問題：「只是比較一下再寫，能有什麼問題？」
     * 但這裡其實有 Race Condition
     */
    if (local_result > stats.max_value) {
        /*
         * 問題在這裡：
         * Thread A 判斷 local_result > max_value（條件成立）
         * Thread B 也判斷 local_result > max_value（條件成立）
         * Thread A 寫入 max_value
         * Thread B 也寫入 max_value（覆蓋了 A）
         *
         * 最終 max_value 可能是 A 或 B 的值
         * 取決於誰最後寫，不可預測
         */
        stats.max_value = local_result;
    }

    printf("Thread %d 完成，local_result = %ld\n", id, local_result);
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

    printf("\n統計結果：\n");
    printf("  總和：    %ld\n", stats.total);
    printf("  完成數：  %d\n",  stats.count);
    printf("  最大值：  %ld\n", stats.max_value);

    pthread_mutex_destroy(&lock);
    return 0;
}