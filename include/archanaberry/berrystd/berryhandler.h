#ifndef BERRYHANDLER_H
#define BERRYHANDLER_H

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <assert.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>
#include <stdatomic.h>
#include <stdarg.h>
#include <stdint.h>

/* 
 * Jika BERRY_MAX_THREADS tidak didefinisikan dari implementasi,
 * maka tidak ada batasan default di sini.
 */
// #ifndef BERRY_MAX_THREADS
//     #define BERRY_MAX_THREADS 8
// #endif

// Aktifkan mode otomatis (auto cleanup)
#define BERRY_AUTOMATIC

// Uncomment untuk mengaktifkan arena allocator (opsional)
// #define BERRY_USE_ARENA

/* Tanpa attribute cleanup – macro didefinisikan sebagai kosong */
#define BERRY_CLEANUP(func)

/* Macro alias untuk "free" manual, mirip seperti drop di Rust */
#define berry_free(mem) berry_release(mem)

/* Definisi owner yang ketat: setiap alokasi harus dimiliki oleh "archanaberry" */
#define ARCHANABERRY_OWNER "archanaberry"

/* Nilai canary untuk proteksi overflow */
#define BERRY_CANARY 0xDEADBEEF
#define BERRY_CANARY_SIZE sizeof(uint32_t)

/* --------------------------------------------------------------------------
 * Logger Callback & Error Handling
 * -------------------------------------------------------------------------- */
typedef void (*BerryLogger)(const char *fmt, ...);
static BerryLogger berry_logger = NULL;
static inline void berry_set_logger(BerryLogger logger) { berry_logger = logger; }
static inline void berry_default_log(const char *fmt, ...) {
#ifdef BERRY_DEBUG
    va_list args; va_start(args, fmt); vfprintf(stderr, fmt, args); va_end(args);
#endif
}

/* --------------------------------------------------------------------------
 * MEMORY HANDLER - Archanaberry Memory (dengan Validasi, Ownership, Canary)
 * -------------------------------------------------------------------------- */
#define BERRY_MAGIC 0xBEEFBEEF

typedef struct ArchanaberryMemory {
    uint32_t magic;         /* untuk validasi pointer */
    void *ptr;              /* pointer aman ke data pengguna (setelah canary pertama) */
    size_t size;            /* ukuran blok memori yang diminta pengguna */
    char owner[16];         /* harus "archanaberry" */
    atomic_int ref_count;   /* atomic reference count */
} ArchanaberryMemory;

/* Registry tracking & free-list untuk node */
typedef struct BerryMemNode {
    ArchanaberryMemory *mem;
    struct BerryMemNode* next;
} BerryMemNode;

static BerryMemNode* berry_memnode_pool = NULL;
static pthread_mutex_t berry_memnode_pool_mutex = PTHREAD_MUTEX_INITIALIZER;

static inline BerryMemNode* berry_alloc_memnode(void) {
    pthread_mutex_lock(&berry_memnode_pool_mutex);
    BerryMemNode* node = berry_memnode_pool;
    if (node) {
        berry_memnode_pool = node->next;
    } else {
        node = malloc(sizeof(BerryMemNode));
        if (!node) {
            pthread_mutex_unlock(&berry_memnode_pool_mutex);
            fprintf(stderr, "Error: Gagal mengalokasikan BerryMemNode: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
    }
    pthread_mutex_unlock(&berry_memnode_pool_mutex);
    return node;
}

static inline void berry_free_memnode(BerryMemNode* node) {
    pthread_mutex_lock(&berry_memnode_pool_mutex);
    node->next = berry_memnode_pool;
    berry_memnode_pool = node;
    pthread_mutex_unlock(&berry_memnode_pool_mutex);
}

/* Registry global untuk tracking alokasi memori */
static BerryMemNode* berry_mem_list = NULL;
static pthread_mutex_t berry_mem_list_mutex = PTHREAD_MUTEX_INITIALIZER;

static inline void berry_register_allocation(ArchanaberryMemory *mem) {
    pthread_mutex_lock(&berry_mem_list_mutex);
    BerryMemNode* node = berry_alloc_memnode();
    node->mem = mem;
    node->next = berry_mem_list;
    berry_mem_list = node;
    pthread_mutex_unlock(&berry_mem_list_mutex);
#ifdef BERRY_DEBUG
    if (berry_logger)
        berry_logger("Register: %p, size: %zu, ref: %d\n", mem->ptr,
                     mem->size, atomic_load(&mem->ref_count));
#endif
}

static inline void berry_unregister_allocation(ArchanaberryMemory *mem) {
    pthread_mutex_lock(&berry_mem_list_mutex);
    BerryMemNode **curr = &berry_mem_list;
    while (*curr) {
        if ((*curr)->mem == mem) {
            BerryMemNode* temp = *curr;
            *curr = temp->next;
            berry_free_memnode(temp);
            break;
        }
        curr = &((*curr)->next);
    }
    pthread_mutex_unlock(&berry_mem_list_mutex);
#ifdef BERRY_DEBUG
    if (berry_logger)
        berry_logger("Unregister: %p\n", mem->ptr);
#endif
}

static inline void berry_check_leaks(void) {
#ifdef BERRY_DEBUG
    pthread_mutex_lock(&berry_mem_list_mutex);
    if (berry_mem_list == NULL) {
        printf("BerryHandler: Tidak ada memory leak terdeteksi.\n");
    } else {
        printf("BerryHandler: Terdeteksi memory leak:\n");
        BerryMemNode* curr = berry_mem_list;
        while (curr) {
            printf(" - Alokasi: %p, Ukuran: %zu, RefCount: %d\n",
                   curr->mem->ptr, curr->mem->size,
                   atomic_load(&curr->mem->ref_count));
            curr = curr->next;
        }
    }
    pthread_mutex_unlock(&berry_mem_list_mutex);
#endif
}

static inline int berry_active_allocations(void) {
    int count = 0;
    pthread_mutex_lock(&berry_mem_list_mutex);
    BerryMemNode* curr = berry_mem_list;
    while (curr) { count++; curr = curr->next; }
    pthread_mutex_unlock(&berry_mem_list_mutex);
    return count;
}

static inline ArchanaberryMemory* berry_alloc(size_t size) {
    if (size == 0 || size > (SIZE_MAX - sizeof(ArchanaberryMemory) - 2 * BERRY_CANARY_SIZE)) {
        fprintf(stderr, "Error: Ukuran alokasi tidak valid.\n");
        exit(EXIT_FAILURE);
    }
    ArchanaberryMemory* mem = malloc(sizeof(ArchanaberryMemory));
    if (!mem) {
        fprintf(stderr, "Error: Gagal mengalokasikan ArchanaberryMemory: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    size_t total_size = size + 2 * BERRY_CANARY_SIZE;
    void *raw = calloc(1, total_size);
    if (!raw) {
        fprintf(stderr, "Error: Gagal mengalokasikan blok memori dengan canary: %s\n", strerror(errno));
        free(mem);
        exit(EXIT_FAILURE);
    }
    *((uint32_t*)raw) = BERRY_CANARY;  /* canary awal */
    *((uint32_t*)((char*)raw + BERRY_CANARY_SIZE + size)) = BERRY_CANARY; /* canary akhir */
    mem->ptr = (void*)((char*)raw + BERRY_CANARY_SIZE);
    mem->size = size;
    strncpy(mem->owner, ARCHANABERRY_OWNER, sizeof(mem->owner) - 1);
    mem->owner[sizeof(mem->owner) - 1] = '\0';
    mem->magic = BERRY_MAGIC;
    atomic_init(&mem->ref_count, 1);
    berry_register_allocation(mem);
    return mem;
}

static inline void berry_retain(ArchanaberryMemory *mem) {
    assert(mem && mem->magic == BERRY_MAGIC && "Pointer invalid atau sudah dibebaskan!");
    if (strcmp(mem->owner, ARCHANABERRY_OWNER) != 0) {
        fprintf(stderr, "Error: Ownership tidak valid pada pointer %p\n", mem->ptr);
        exit(EXIT_FAILURE);
    }
    atomic_fetch_add(&mem->ref_count, 1);
}

static inline void berry_release(ArchanaberryMemory *mem) {
    assert(mem && mem->magic == BERRY_MAGIC && "Pointer invalid atau sudah dibebaskan!");
    if (strcmp(mem->owner, ARCHANABERRY_OWNER) != 0) {
        fprintf(stderr, "Error: Ownership tidak valid pada pointer %p\n", mem->ptr);
        exit(EXIT_FAILURE);
    }
    void* raw = (char*)mem->ptr - BERRY_CANARY_SIZE;
    uint32_t canary_begin = *((uint32_t*)raw);
    uint32_t canary_end = *((uint32_t*)((char*)raw + BERRY_CANARY_SIZE + mem->size));
    if (canary_begin != BERRY_CANARY || canary_end != BERRY_CANARY) {
        fprintf(stderr, "Error: Deteksi korupsi memori (canary gagal) pada pointer %p\n", mem->ptr);
        exit(EXIT_FAILURE);
    }
    if (atomic_fetch_sub(&mem->ref_count, 1) == 1) {
        berry_unregister_allocation(mem);
        free((char*)mem->ptr - BERRY_CANARY_SIZE);
        mem->magic = 0;
        free(mem);
    }
}

#define berry_borrow berry_retain
#define berry_return berry_release

static inline void berry_release_cleanup(ArchanaberryMemory **mem) {
    if (mem && *mem) {
        berry_release(*mem);
        *mem = NULL;
    }
}

#ifdef BERRY_AUTOMATIC
    #define BERRY_AUTO_ALLOC(var, size) \
        ArchanaberryMemory *var = berry_alloc(size)
#else
    #define BERRY_AUTO_ALLOC(var, size) \
        ArchanaberryMemory *var = berry_alloc(size)
#endif

#define BERRY_MANUAL_ALLOC(var, size) \
    ArchanaberryMemory *var = berry_alloc(size)

/* --------------------------------------------------------------------------
 * MODE OTOMATIS: BLOCK RUNNER UNTUK THREAD POOL
 * -------------------------------------------------------------------------- */
#ifdef BERRY_AUTOMATIC
    #define ARCHANABERRY_AUTO_RUN(num_threads, code_block)      \
        do {                                                    \
            BerryThreadPool pool;                               \
            archanaberry_start(&pool, num_threads);             \
            { code_block }                                      \
            archanaberry_finish(&pool);                         \
        } while(0)
#endif

/* --------------------------------------------------------------------------
 * ARENA ALLOCATOR (OPSIONAL)
 * -------------------------------------------------------------------------- */
#ifdef BERRY_USE_ARENA
typedef struct BerryArena {
    void *memory;
    size_t capacity;
    size_t offset;
} BerryArena;

static inline BerryArena* berry_arena_create(size_t capacity) {
    if (capacity == 0) {
        fprintf(stderr, "Error: Kapasitas arena tidak boleh 0.\n");
        exit(EXIT_FAILURE);
    }
    BerryArena* arena = malloc(sizeof(BerryArena));
    if (!arena) {
        fprintf(stderr, "Error: Gagal mengalokasikan BerryArena: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    arena->memory = malloc(capacity);
    if (!arena->memory) {
        fprintf(stderr, "Error: Gagal mengalokasikan memori arena: %s\n", strerror(errno));
        free(arena);
        exit(EXIT_FAILURE);
    }
    arena->capacity = capacity;
    arena->offset = 0;
    return arena;
}

static inline void* berry_arena_alloc(BerryArena *arena, size_t size) {
    if (arena->offset + size > arena->capacity) {
        fprintf(stderr, "Error: Kapasitas arena tidak mencukupi (offset %zu, size %zu, capacity %zu)\n",
                arena->offset, size, arena->capacity);
        return NULL;
    }
    void *ptr = (char*)arena->memory + arena->offset;
    arena->offset += size;
    return ptr;
}

static inline void berry_arena_reset(BerryArena *arena) { arena->offset = 0; }
static inline void berry_arena_destroy(BerryArena *arena) {
    if (arena) { free(arena->memory); free(arena); }
}
#endif /* BERRY_USE_ARENA */

/* --------------------------------------------------------------------------
 * THREAD POOL & SCHEDULER
 * -------------------------------------------------------------------------- */
typedef void (*TaskFunction)(void*);

typedef struct Task {
    TaskFunction function;
    void* argument;
    int priority;  /* Semakin tinggi, semakin tinggi prioritas */
    bool cancelled;
    struct Task* next;
} Task;

typedef struct {
    pthread_t *threads; /* Alokasi thread dinamis berdasarkan parameter num_threads */
    Task* task_queue;
    pthread_mutex_t queue_lock;
    pthread_cond_t condition;
    bool stop;
    int num_threads;
    atomic_int tasks_executed;
} BerryThreadPool;

static Task* task_pool = NULL;
static pthread_mutex_t task_pool_mutex = PTHREAD_MUTEX_INITIALIZER;

static inline Task* berry_alloc_task(void) {
    pthread_mutex_lock(&task_pool_mutex);
    Task* t = task_pool;
    if (t) { task_pool = t->next; }
    else {
        t = malloc(sizeof(Task));
        if (!t) {
            pthread_mutex_unlock(&task_pool_mutex);
            fprintf(stderr, "Error: Gagal mengalokasikan Task: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
    }
    pthread_mutex_unlock(&task_pool_mutex);
    t->next = NULL;
    t->cancelled = false;
    t->priority = 0;
    return t;
}

static inline void berry_free_task(Task* t) {
    pthread_mutex_lock(&task_pool_mutex);
    t->next = task_pool;
    task_pool = t;
    pthread_mutex_unlock(&task_pool_mutex);
}

static inline void init_thread_pool(BerryThreadPool* pool, int num_threads) {
    assert(pool != NULL);
    pool->num_threads = num_threads;
    pool->task_queue = NULL;
    pool->stop = false;
    atomic_init(&pool->tasks_executed, 0);
    pool->threads = malloc(sizeof(pthread_t) * num_threads);
    if (!pool->threads) {
        fprintf(stderr, "Error: Gagal mengalokasikan array thread: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    if (pthread_mutex_init(&(pool->queue_lock), NULL) != 0) {
        fprintf(stderr, "Error: Gagal inisialisasi mutex: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    if (pthread_cond_init(&(pool->condition), NULL) != 0) {
        fprintf(stderr, "Error: Gagal inisialisasi condition variable: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
}

static inline void add_task(BerryThreadPool* pool, TaskFunction func, void* arg, int priority) {
    assert(pool != NULL && func != NULL);
    Task* new_task = berry_alloc_task();
    new_task->function = func;
    new_task->argument = arg;
    new_task->priority = priority;
    new_task->cancelled = false;
    new_task->next = NULL;

    pthread_mutex_lock(&(pool->queue_lock));
    if (pool->task_queue == NULL || new_task->priority > pool->task_queue->priority) {
        new_task->next = pool->task_queue;
        pool->task_queue = new_task;
    } else {
        Task* temp = pool->task_queue;
        while (temp->next && temp->next->priority >= new_task->priority) {
            temp = temp->next;
        }
        new_task->next = temp->next;
        temp->next = new_task;
    }
    pthread_cond_signal(&(pool->condition));
    pthread_mutex_unlock(&(pool->queue_lock));
}

static inline void cancel_task(BerryThreadPool* pool, TaskFunction func, void* arg) {
    pthread_mutex_lock(&(pool->queue_lock));
    Task* curr = pool->task_queue;
    while (curr) {
        if (curr->function == func && curr->argument == arg)
            curr->cancelled = true;
        curr = curr->next;
    }
    pthread_mutex_unlock(&(pool->queue_lock));
}

static inline void* thread_worker(void* arg) {
    BerryThreadPool* pool = (BerryThreadPool*)arg;
    while (1) {
        pthread_mutex_lock(&(pool->queue_lock));
        while (pool->task_queue == NULL && !pool->stop)
            pthread_cond_wait(&(pool->condition), &(pool->queue_lock));
        if (pool->stop && pool->task_queue == NULL) {
            pthread_mutex_unlock(&(pool->queue_lock));
            break;
        }
        Task* task = pool->task_queue;
        if (task) pool->task_queue = task->next;
        pthread_mutex_unlock(&(pool->queue_lock));
        if (task) {
            if (!task->cancelled) {
                task->function(task->argument);
                atomic_fetch_add(&pool->tasks_executed, 1);
            }
            berry_free_task(task);
        }
    }
    return NULL;
}

static inline void start_thread_pool(BerryThreadPool* pool) {
    assert(pool != NULL);
    for (int i = 0; i < pool->num_threads; i++) {
        if (pthread_create(&(pool->threads[i]), NULL, thread_worker, pool) != 0) {
            fprintf(stderr, "Error: Gagal membuat thread %d: %s\n", i, strerror(errno));
            exit(EXIT_FAILURE);
        }
    }
}

static inline void wait_for_tasks(BerryThreadPool* pool) {
    while (1) {
        pthread_mutex_lock(&(pool->queue_lock));
        bool empty = (pool->task_queue == NULL);
        pthread_mutex_unlock(&(pool->queue_lock));
        if (empty) break;
        usleep(1000);
    }
}

static inline void stop_thread_pool(BerryThreadPool* pool) {
    assert(pool != NULL);
    pthread_mutex_lock(&(pool->queue_lock));
    pool->stop = true;
    pthread_cond_broadcast(&(pool->condition));
    pthread_mutex_unlock(&(pool->queue_lock));
    for (int i = 0; i < pool->num_threads; i++)
        pthread_join(pool->threads[i], NULL);
    while (pool->task_queue) {
        Task* temp = pool->task_queue;
        pool->task_queue = pool->task_queue->next;
        berry_free_task(temp);
    }
    pthread_mutex_destroy(&(pool->queue_lock));
    pthread_cond_destroy(&(pool->condition));
    free(pool->threads);
}

static inline void berry_task_stats(BerryThreadPool* pool, int *pending, int *executed) {
    int count = 0;
    pthread_mutex_lock(&(pool->queue_lock));
    Task* curr = pool->task_queue;
    while (curr) { count++; curr = curr->next; }
    pthread_mutex_unlock(&(pool->queue_lock));
    if (pending) *pending = count;
    if (executed) *executed = atomic_load(&pool->tasks_executed);
}

/* --------------------------------------------------------------------------
 * TREE & RECURSIVE CRAWLING (Utility)
 * -------------------------------------------------------------------------- */
typedef struct Node {
    int value;
    struct Node* left;
    struct Node* right;
} Node;

static inline void archanaberry_crawl(Node* root) {
    if (root == NULL) return;
    archanaberry_crawl(root->left);
    printf("Node value: %d\n", root->value);
    archanaberry_crawl(root->right);
}

static inline Node* create_node(int value) {
    Node* node = malloc(sizeof(Node));
    if (!node) {
        fprintf(stderr, "Error: Gagal mengalokasikan node: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    node->value = value;
    node->left = NULL;
    node->right = NULL;
    return node;
}

static inline void free_tree(Node* root) {
    if (root == NULL) return;
    free_tree(root->left);
    free_tree(root->right);
    free(root);
}

/* --------------------------------------------------------------------------
 * UTILITY TASKS & ALIASES
 * -------------------------------------------------------------------------- */
typedef struct {
    char *start;
    size_t length;
} WriteTaskArg;

static inline void berry_write_task(void *arg) {
    WriteTaskArg *warg = (WriteTaskArg *) arg;
    memset(warg->start, 0x55, warg->length);
    free(warg);
}

/* --------------------------------------------------------------------------
 * THREAD POOL PENGENDALIAN: START & FINISH (Manual Mode)
 * -------------------------------------------------------------------------- */
static inline void archanaberry_start(BerryThreadPool* pool, int num_threads) {
    init_thread_pool(pool, num_threads);
    start_thread_pool(pool);
}

static inline void archanaberry_finish(BerryThreadPool* pool) {
    wait_for_tasks(pool);
    stop_thread_pool(pool);
}

#endif // BERRYHANDLER_H
