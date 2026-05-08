#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>

void* custom_malloc(size_t size);
void  custom_free(void* ptr);
void* custom_realloc(void* ptr, size_t size);
void* custom_calloc(size_t num, size_t size);

extern void* heap_start;

// returns current time in milliseconds
static double now_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);

    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

#define METADATA_SIZE sizeof(struct memory_block)

typedef struct memory_block* block_ptr;

// structure representing a block in heap
struct memory_block {
    int is_free;
    size_t block_size;
    block_ptr next_block;
    block_ptr prev_block;
    void* memory_ptr;
    char memory[1];
};

// calculates fragmentation percentage
double fragmentation_ratio() {
    block_ptr cur = (block_ptr)heap_start;

    size_t free_bytes = 0;
    size_t total_bytes = 0;

    while (cur) {
        total_bytes += cur->block_size + METADATA_SIZE;

        if (cur->is_free) {
            free_bytes += cur->block_size;
        }

        cur = cur->next_block;
    }

    if (total_bytes == 0) {
        return 0.0;
    }

    return (double)free_bytes / (double)total_bytes * 100.0;
}

#define SEED 42
#define OPS 10000
#define POOL_SIZE 256
#define MAX_ALLOC 512

typedef void* (*malloc_fn)(size_t);
typedef void  (*free_fn)(void*);
typedef void* (*realloc_fn)(void*, size_t);

typedef struct {
    double ms;
    double frag;
    long ops;
} Result;

// runs randomized allocator benchmark
Result run_benchmark(
    const char* label,
    malloc_fn my_malloc,
    free_fn my_free,
    realloc_fn my_realloc,
    int compute_frag
) {
    void* ptrs[POOL_SIZE];
    size_t sizes[POOL_SIZE];

    memset(ptrs, 0, sizeof(ptrs));
    memset(sizes, 0, sizeof(sizes));

    srand(SEED);

    long ops = 0;

    double start = now_ms();

    for (int i = 0; i < OPS; i++) {
        int slot = rand() % POOL_SIZE;
        int op = rand() % 3;

        size_t sz = (rand() % MAX_ALLOC) + 1;

        if (op == 0 || ptrs[slot] == NULL) {

            if (ptrs[slot]) {
                my_free(ptrs[slot]);
            }

            ptrs[slot] = my_malloc(sz);
            sizes[slot] = sz;

            if (ptrs[slot]) {
                memset(ptrs[slot], 0xAB, sz);
            }

        } else if (op == 1) {

            my_free(ptrs[slot]);

            ptrs[slot] = NULL;
            sizes[slot] = 0;

        } else {

            void* np = my_realloc(ptrs[slot], sz);

            if (np) {
                ptrs[slot] = np;
                sizes[slot] = sz;
            }
        }

        ops++;
    }

    double elapsed = now_ms() - start;

    double frag = compute_frag ? fragmentation_ratio() : -1.0;

    for (int i = 0; i < POOL_SIZE; i++) {
        if (ptrs[i]) {
            my_free(ptrs[i]);
        }
    }

    printf("  %-20s  %6.2f ms   %6ld ops   frag: %s\n",
           label,
           elapsed,
           ops,
           frag >= 0 ? (char[16]){0} : "n/a (glibc)");

    if (frag >= 0) {
        printf("  %-20s  fragmentation after cleanup: %.2f%%\n",
               "",
               frag);
    }

    return (Result){ elapsed, frag, ops };
}

// tests allocator edge cases
void test_edge_cases() {

    void* p = custom_malloc(0);

    printf("  malloc(0)           → %s\n",
           p ? "non-NULL (ok)" : "NULL");

    if (p) {
        custom_free(p);
    }

    custom_free(NULL);

    printf("  free(NULL)          → no crash ✓\n");

    p = custom_realloc(NULL, 64);

    printf("  realloc(NULL,64)    → %s\n",
           p ? "allocated ✓" : "FAILED ✗");

    if (p) {
        custom_free(p);
    }

    p = custom_malloc(32);

    void* q = custom_realloc(p, 0);

    printf("  realloc(ptr,0)      → %s\n",
           !q ? "NULL (freed) ✓" : "non-NULL");

    void* c = custom_calloc(SIZE_MAX, 2);

    printf("  calloc(SIZE_MAX,2)  → %s\n",
           !c ? "NULL ✓" : "FAILED ✗");

    p = custom_malloc(16);

    custom_free(p);
    custom_free(p);

    printf("  double-free         → no crash ✓\n");

    int* arr = (int*)custom_malloc(100 * sizeof(int));

    if (arr) {

        for (int i = 0; i < 100; i++) {
            arr[i] = i * 3;
        }

        int ok = 1;

        for (int i = 0; i < 100; i++) {
            if (arr[i] != i * 3) {
                ok = 0;
                break;
            }
        }

        printf("  write/read 100 ints → %s\n",
               ok ? "correct ✓" : "CORRUPTED ✗");

        custom_free(arr);
    }
}

int main() {

    printf("── Custom Allocator Benchmark (%d ops, pool=%d, max_alloc=%dB) ──\n\n",
           OPS,
           POOL_SIZE,
           MAX_ALLOC);

    printf("Performance:\n");

    Result custom = run_benchmark(
        "custom (first-fit)",
        custom_malloc,
        custom_free,
        custom_realloc,
        1
    );

    Result glibc = run_benchmark(
        "glibc malloc",
        malloc,
        free,
        realloc,
        0
    );

    printf("\nSummary:\n");

    printf("  Throughput  — custom: %.0f ops/ms   glibc: %.0f ops/ms\n",
           custom.ops / custom.ms,
           glibc.ops / glibc.ms);

    if (custom.ms < glibc.ms) {

        printf("  Custom is %.1fx faster on this workload\n",
               glibc.ms / custom.ms);

    } else {

        printf("  glibc is %.1fx faster\n",
               custom.ms / glibc.ms);
    }

    if (custom.frag >= 0) {

        printf("  Post-cleanup fragmentation: %.2f%%\n",
               custom.frag);
    }

    test_edge_cases();

    printf("\nDone.\n");

    return 0;
}