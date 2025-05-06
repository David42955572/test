#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

int sum; /* this data is shared by the thread(s) */
int original_list[] = {7, 12, 19, 3, 18, 4, 2, 6, 15, 8};  // 假設這是原始列表
#define N_LIST (sizeof(original_list)/sizeof(int)) // 計算列表的元素數量
int mylist[N_LIST];  // 用來存儲複製後的列表

/* 這個函數將原始列表複製到新列表 */
int *listncopy(int *dst, int *src, int n) {
    for (int i = 0; i < n; i++) {
        dst[i] = src[i];
    }
    return dst;
}

/* 列印列表的函數 */
void print_list(char *id, char *msg, int *first, int n) {
    printf("%s %s: ", id, msg);
    for (int i = 0; i < n; i++) {
        printf("%d ", first[i]);
    }
    printf("\n");
}

/* 線程執行的函數 */
void *runner(void *param) {
    int i, upper = atoi((char *)param);
    sum = 0;
    for (i = 1; i <= upper; i++) {
        sum += i;
    }
    pthread_exit(0);
}

int main(int argc, char *argv[]) {
    pthread_t tid;  /* 線程識別符 */
    pthread_attr_t attr;  /* 線程屬性 */

    /* 設置默認的線程屬性 */
    pthread_attr_init(&attr);

    /* 複製原始列表到 mylist */
    listncopy(mylist, original_list, N_LIST);
    
    /* 顯示原始列表 */
    print_list("A1115545", "Original List", mylist, N_LIST);

    /* 檢查命令行參數並創建線程 */
    if (argc > 1) {
        /* 創建線程，並將命令行參數傳遞給線程 */
        pthread_create(&tid, &attr, runner, argv[1]);

        /* 等待線程執行完成 */
        pthread_join(tid, NULL);

        /* 顯示結果 */
        printf("A1115545 sum = %d\n", sum);
    } else {
        printf("Please provide a number as argument.\n");
    }

    return 0;
}
