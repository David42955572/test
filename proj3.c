#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>
#include <string.h>

#define NAME "A1115545"  // ← 這裡請改成你的學號
#define N_LIST 10

int original_list[] = {7, 12, 19, 3, 18, 4, 2, 6, 15, 8};
int sorted_list[N_LIST];

typedef struct {
    char id[32];
    int *data;
    int size;
    long usec;
} ThreadArg;

long usec_elapsed(struct timeval s, struct timeval e) {
    return 1000000 * (e.tv_sec - s.tv_sec) + (e.tv_usec - s.tv_usec);
}

int *listncopy(int *dst, int *src, int n) {
    for (int i = 0; i < n; i++) dst[i] = src[i];
    return dst;
}

void print_list(const char *id, const char *msg, int *first, int n) {
    printf("%s %s:", id, msg);
    for (int i = 0; i < n; i++) printf(" %d", first[i]);
    printf("\n");
}

int compare(const void *a, const void *b) {
    return (*(int *)a - *(int *)b);
}

void *do_sort(void *arg) {
    ThreadArg *targ = (ThreadArg *)arg;
    struct timeval start, end;
    print_list(targ->id, "Sub-Old", targ->data, targ->size);
    gettimeofday(&start, NULL);

    // Insertion Sort
    for (int i = 1; i < targ->size; i++) {
        int key = targ->data[i];
        int j = i - 1;
        while (j >= 0 && targ->data[j] > key) {
            targ->data[j + 1] = targ->data[j];
            j--;
        }
        targ->data[j + 1] = key;
    }

    gettimeofday(&end, NULL);
    print_list(targ->id, "Sub-New", targ->data, targ->size);
    targ->usec = usec_elapsed(start, end);
    printf("%s spent %ld usec\n", targ->id, targ->usec);
    pthread_exit(NULL);
}


void *do_merge(void *arg) {
    ThreadArg *targ = (ThreadArg *)arg;
    struct timeval start, end;
    int mid = targ->size / 2;
    int i = 0, j = mid, k = 0;
    int *src = targ->data;
    int temp[N_LIST];

    gettimeofday(&start, NULL);
    while (i < mid && j < targ->size) {
        if (src[i] <= src[j])
            temp[k++] = src[i++];
        else
            temp[k++] = src[j++];
    }
    while (i < mid) temp[k++] = src[i++];
    while (j < targ->size) temp[k++] = src[j++];
    for (i = 0; i < targ->size; i++) src[i] = temp[i];
    gettimeofday(&end, NULL);
    targ->usec = usec_elapsed(start, end);
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <upper>\n", argv[0]);
        exit(1);
    }

    int upper = atoi(argv[1]);
    int sum = 0;
    for (int i = 1; i <= upper; i++)
        sum += i;

    printf("%s sum = %d\n", NAME, sum);

    // Step 1: 複製原始陣列
    listncopy(sorted_list, original_list, N_LIST);
    print_list(NAME "-M", "All-Old", sorted_list, N_LIST);

    // Step 2 & 3: Create sorting threads
    pthread_t tid1, tid2, tidm;
    ThreadArg arg1 = {.data = sorted_list, .size = N_LIST / 2};
    ThreadArg arg2 = {.data = sorted_list + N_LIST / 2, .size = N_LIST / 2};
    ThreadArg argm = {.data = sorted_list, .size = N_LIST};

    snprintf(arg1.id, sizeof(arg1.id), "%s#0", NAME);
    snprintf(arg2.id, sizeof(arg2.id), "%s#1", NAME);
    snprintf(argm.id, sizeof(argm.id), "%s#M", NAME);

    pthread_create(&tid1, NULL, do_sort, &arg1);
    pthread_create(&tid2, NULL, do_sort, &arg2);
    pthread_join(tid1, NULL);
    pthread_join(tid2, NULL);

    // Step 4: Merging thread
    pthread_create(&tidm, NULL, do_merge, &argm);
    pthread_join(tidm, NULL);

    // Final output
    print_list(NAME "-M", "All-New", sorted_list, N_LIST);
    printf("%s-M spent %ld usec\n", NAME, arg1.usec + arg2.usec + argm.usec);

    return 0;
}
