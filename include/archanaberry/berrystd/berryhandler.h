#ifndef BERRYHANDLER_H
#define BERRYHANDLER_H

// Konfigurasi
#ifndef BERRY_MAX_THREADS
#define BERRY_MAX_THREADS 8
#endif

// Uncomment untuk mode debug (menampilkan log dan leak-checking)
// #define BERRY_DEBUG

// Uncomment untuk mode otomatis (variabel dan fungsi auto cleanup)
// #define BERRY_AUTOMATIC

// Jika menggunakan compiler GCC/Clang, aktifkan cleanup attribute (opsional)
#ifdef __GNUC__
#define BERRY_CLEANUP(func) __attribute__((cleanup(func)))
#else
#define BERRY_CLEANUP(func)
#endif

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

// ===============================
// Logging Callback & Error Handling
// ===============================

typedef void (*BerryLogger)(const char *fmt, ...);
static BerryLogger berry_logger = NULL;

static inline void berry_set_logger(BerryLogger logger) {
    berry_logger = logger;
}

static inline void berry_default_log(const char *fmt, ...) {
#ifdef BERRY_DEBUG
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
#endif
}

// ===============================
// MEMORY HANDLER (Archanaberry)
// Smart allocation dengan reference counting dan registry
// ===============================

// Struktur untuk alokasi memori.
typedef struct ArchanaberryMemory {
    void *ptr;              // Pointer ke blok memori
    size_t size;            // Ukuran blok memori
    int owner_id;           // ID pemilik (simulasi)
    atomic_int ref_count;   // Reference count (atomic)
} ArchanaberryMemory;

// --- Memory Pool untuk Registry Node (BerryMemNode) ---
typedef struct BerryMemNode {
    ArchanaberryMemory *mem;
    struct BerryMemNode* next;
} BerryMemNode;

// Free-list sederhana untuk BerryMemNode
static BerryMemNode* berry_memnode_pool = NULL;
static pthread_mutex_t berry_memnode_pool_mutex = PTHREAD_MUTEX_INITIALIZER;

// Dapatkan node dari pool atau alokasikan baru
static inline BerryMemNode* berry_alloc_memnode(void) {
    pthread_mutex_lock(&berry_memnode_pool_mutex);
    BerryMemNode* node = berry_memnode_pool;
    if (node) {
        berry_memnode_pool = node->next;
    } else {
        node = malloc(sizeof(BerryMemNode));
        if (!node) {
            pthread_mutex_unlock(&berry_memnode_pool_mutex);
            fprintf(stderr, "Error: Gagal alokasikan BerryMemNode: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
    }
    pthread_mutex_unlock(&berry_memnode_pool_mutex);
    return node;
}

// Kembalikan node ke pool
static inline void berry_free_memnode(BerryMemNode* node) {
    pthread_mutex_lock(&berry_memnode_pool_mutex);
    node->next = berry_memnode_pool;
    berry_memnode_pool = node;
    pthread_mutex_unlock(&berry_memnode_pool_mutex);
}

// Registry global untuk tracking alokasi
static BerryMemNode* berry_mem_list = NULL;
static pthread_mutex_t berry_mem_list_mutex = PTHREAD_MUTEX_INITIALIZER;

// Menambahkan blok memori ke registry global.
static inline void berry_register_allocation(ArchanaberryMemory *mem) {
    pthread_mutex_lock(&berry_mem_list_mutex);
    BerryMemNode* node = berry_alloc_memnode();
    node->mem = mem;
    node->next = berry_mem_list;
    berry_mem_list = node;
    pthread_mutex_unlock(&berry_mem_list_mutex);
#ifdef BERRY_DEBUG
    if (berry_logger)
        berry_logger("Register: %p, size: %zu, ref: %d\n", mem->ptr, mem->size, atomic_load(&mem->ref_count));
#endif
}

// Menghapus blok memori dari registry global.
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

// Mengecek dan mencetak blok memori yang belum dibebaskan (jika ada).
static inline void berry_check_leaks() {
#ifdef BERRY_DEBUG
    pthread_mutex_lock(&berry_mem_list_mutex);
    if (berry_mem_list == NULL) {
        printf("BerryHandler: Tidak ada memory leak terdeteksi.\n");
    } else {
        printf("BerryHandler: Terdeteksi memory leak:\n");
        BerryMemNode* curr = berry_mem_list;
        while (curr) {
            printf(" - Alokasi: %p, Ukuran: %zu, RefCount: %d\n",
                   curr->mem->ptr, curr->mem->size, atomic_load(&curr->mem->ref_count));
            curr = curr->next;
        }
    }
    pthread_mutex_unlock(&berry_mem_list_mutex);
#endif
}

// Fungsi statistik: hitung jumlah alokasi aktif
static inline int berry_active_allocations() {
    int count = 0;
    pthread_mutex_lock(&berry_mem_list_mutex);
    BerryMemNode* curr = berry_mem_list;
    while (curr) { count++; curr = curr->next; }
    pthread_mutex_unlock(&berry_mem_list_mutex);
    return count;
}

// Mengalokasikan blok memori baru. Inisialisasi ref_count ke 1.
static inline ArchanaberryMemory* berry_alloc(size_t size, int owner_id) {
    ArchanaberryMemory* mem = malloc(sizeof(ArchanaberryMemory));
    if (!mem) {
        fprintf(stderr, "Error: Gagal mengalokasikan ArchanaberryMemory: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    mem->ptr = malloc(size);
    if (!mem->ptr) {
        fprintf(stderr, "Error: Gagal mengalokasikan blok memori: %s\n", strerror(errno));
        free(mem);
        exit(EXIT_FAILURE);
    }
    mem->size = size;
    mem->owner_id = owner_id;
    atomic_init(&mem->ref_count, 1);
    berry_register_allocation(mem);
    return mem;
}

// Meningkatkan reference count (retain).
static inline void berry_retain(ArchanaberryMemory *mem) {
    assert(mem != NULL);
    atomic_fetch_add(&mem->ref_count, 1);
}

// Mengurangi reference count (release) dan membebaskan memori bila count mencapai 0.
static inline void berry_release(ArchanaberryMemory *mem) {
    assert(mem != NULL);
    if (atomic_fetch_sub(&mem->ref_count, 1) == 1) {
        berry_unregister_allocation(mem);
        free(mem->ptr);
        free(mem);
    }
}

#define berry_borrow berry_retain
#define berry_return berry_release

// Fungsi cleanup untuk ArchanaberryMemory
static inline void berry_release_cleanup(ArchanaberryMemory **mem) {
    if (mem && *mem) {
        berry_release(*mem);
        *mem = NULL;
    }
}

/* ===============================
   MAKRO ALOKASI MODE OTOMATIS/MANUAL
   - Jika BERRY_AUTOMATIC didefinisikan, maka alokasi dengan BERRY_AUTO_ALLOC
     akan secara otomatis dipanggil cleanup-nya saat variabel keluar scope.
   - Jika manual, gunakan BERRY_MANUAL_ALLOC dan panggil berry_release() secara eksplisit.
=============================== */

#ifdef BERRY_AUTOMATIC
    #define BERRY_AUTO_ALLOC(var, size, owner) \
        ArchanaberryMemory *var BERRY_CLEANUP(berry_release_cleanup) = berry_alloc(size, owner)
#else
    #define BERRY_AUTO_ALLOC(var, size, owner) \
        ArchanaberryMemory *var = berry_alloc(size, owner)
#endif

// Untuk mode manual, makro ini hanyalah alias dari berry_alloc()
#define BERRY_MANUAL_ALLOC(var, size, owner) \
    ArchanaberryMemory *var = berry_alloc(size, owner)

/* ===============================
   THREAD POOL & SCHEDULER
=============================== */

// Deklarasikan tipe TaskFunction terlebih dahulu.
typedef void (*TaskFunction)(void*);

// --- Memory Pool untuk Task ---
typedef struct Task {
    TaskFunction function;
    void* argument;
    int priority;      // Nilai prioritas (semakin besar, semakin tinggi prioritas)
    bool cancelled;    // Flag pembatalan task
    struct Task* next;
} Task;

typedef struct {
    pthread_t threads[BERRY_MAX_THREADS];
    Task* task_queue;
    pthread_mutex_t queue_lock;
    pthread_cond_t condition;
    bool stop;
    int num_threads;
    atomic_int tasks_executed; // Statistik jumlah task yang telah diproses
} BerryThreadPool;

// Free-list sederhana untuk Task
static Task* task_pool = NULL;
static pthread_mutex_t task_pool_mutex = PTHREAD_MUTEX_INITIALIZER;

// Ambil task dari pool atau alokasikan baru
static inline Task* berry_alloc_task(void) {
    pthread_mutex_lock(&task_pool_mutex);
    Task* t = task_pool;
    if (t) {
        task_pool = t->next;
    } else {
        t = malloc(sizeof(Task));
        if (!t) {
            pthread_mutex_unlock(&task_pool_mutex);
            fprintf(stderr, "Error: Gagal alokasikan Task: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
    }
    pthread_mutex_unlock(&task_pool_mutex);
    t->next = NULL;
    t->cancelled = false;
    t->priority = 0;
    return t;
}

// Kembalikan task ke pool
static inline void berry_free_task(Task* t) {
    pthread_mutex_lock(&task_pool_mutex);
    t->next = task_pool;
    task_pool = t;
    pthread_mutex_unlock(&task_pool_mutex);
}

// Inisialisasi thread pool.
static inline void init_thread_pool(BerryThreadPool* pool, int num_threads) {
    assert(pool != NULL);
    pool->num_threads = (num_threads > BERRY_MAX_THREADS) ? BERRY_MAX_THREADS : num_threads;
    pool->task_queue = NULL;
    pool->stop = false;
    atomic_init(&pool->tasks_executed, 0);
    if (pthread_mutex_init(&(pool->queue_lock), NULL) != 0) {
        fprintf(stderr, "Error: Gagal inisialisasi mutex: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    if (pthread_cond_init(&(pool->condition), NULL) != 0) {
        fprintf(stderr, "Error: Gagal inisialisasi condition variable: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
}

// Menambahkan task ke thread pool dengan dukungan prioritas.
static inline void add_task(BerryThreadPool* pool, TaskFunction func, void* arg, int priority) {
    assert(pool != NULL && func != NULL);
    Task* new_task = berry_alloc_task();
    new_task->function = func;
    new_task->argument = arg;
    new_task->priority = priority;
    new_task->cancelled = false;
    new_task->next = NULL;
    
    pthread_mutex_lock(&(pool->queue_lock));
    // Sisipkan sesuai prioritas (simple insertion sort)
    if (pool->task_queue == NULL || new_task->priority > pool->task_queue->priority) {
        new_task->next = pool->task_queue;
        pool->task_queue = new_task;
    } else {
        Task* temp = pool->task_queue;
        while (temp->next != NULL && temp->next->priority >= new_task->priority) {
            temp = temp->next;
        }
        new_task->next = temp->next;
        temp->next = new_task;
    }
    pthread_cond_signal(&(pool->condition));
    pthread_mutex_unlock(&(pool->queue_lock));
}

// Membatalkan task yang belum dieksekusi berdasarkan fungsi dan argumen (sederhana).
static inline void cancel_task(BerryThreadPool* pool, TaskFunction func, void* arg) {
    pthread_mutex_lock(&(pool->queue_lock));
    Task* curr = pool->task_queue;
    while (curr) {
        if (curr->function == func && curr->argument == arg) {
            curr->cancelled = true;
        }
        curr = curr->next;
    }
    pthread_mutex_unlock(&(pool->queue_lock));
}

// Fungsi worker untuk setiap thread.
static inline void* thread_worker(void* arg) {
    BerryThreadPool* pool = (BerryThreadPool*)arg;
    while (1) {
        pthread_mutex_lock(&(pool->queue_lock));
        while (pool->task_queue == NULL && !pool->stop) {
            pthread_cond_wait(&(pool->condition), &(pool->queue_lock));
        }
        if (pool->stop && pool->task_queue == NULL) {
            pthread_mutex_unlock(&(pool->queue_lock));
            break;
        }
        Task* task = pool->task_queue;
        if (task != NULL) {
            pool->task_queue = task->next;
        }
        pthread_mutex_unlock(&(pool->queue_lock));
        if (task != NULL) {
            if (!task->cancelled) {
                task->function(task->argument);
                atomic_fetch_add(&pool->tasks_executed, 1);
            }
            berry_free_task(task);
        }
    }
    return NULL;
}

// Memulai thread pool.
static inline void start_thread_pool(BerryThreadPool* pool) {
    assert(pool != NULL);
    for (int i = 0; i < pool->num_threads; i++) {
        if (pthread_create(&(pool->threads[i]), NULL, thread_worker, pool) != 0) {
            fprintf(stderr, "Error: Gagal membuat thread %d: %s\n", i, strerror(errno));
            exit(EXIT_FAILURE);
        }
    }
}

// Menunggu hingga antrian task kosong.
static inline void wait_for_tasks(BerryThreadPool* pool) {
    while (1) {
        pthread_mutex_lock(&(pool->queue_lock));
        bool empty = (pool->task_queue == NULL);
        pthread_mutex_unlock(&(pool->queue_lock));
        if (empty)
            break;
        usleep(1000); // Tidur 1ms untuk mencegah busy-wait.
    }
}

// Menghentikan thread pool dan membersihkan task yang tersisa.
static inline void stop_thread_pool(BerryThreadPool* pool) {
    assert(pool != NULL);
    pthread_mutex_lock(&(pool->queue_lock));
    pool->stop = true;
    pthread_cond_broadcast(&(pool->condition));
    pthread_mutex_unlock(&(pool->queue_lock));
    for (int i = 0; i < pool->num_threads; i++) {
        pthread_join(pool->threads[i], NULL);
    }
    while (pool->task_queue != NULL) {
        Task* temp = pool->task_queue;
        pool->task_queue = pool->task_queue->next;
        berry_free_task(temp);
    }
    pthread_mutex_destroy(&(pool->queue_lock));
    pthread_cond_destroy(&(pool->condition));
}

// Fungsi statistik: jumlah task tersisa dan jumlah task dieksekusi.
static inline void berry_task_stats(BerryThreadPool* pool, int *pending, int *executed) {
    int count = 0;
    pthread_mutex_lock(&(pool->queue_lock));
    Task* curr = pool->task_queue;
    while (curr) { count++; curr = curr->next; }
    pthread_mutex_unlock(&(pool->queue_lock));
    if (pending) *pending = count;
    if (executed) *executed = atomic_load(&pool->tasks_executed);
}

/* ===============================
   TREE & RECURSIVE CRAWLING
=============================== */

typedef struct Node {
    int value;
    struct Node* left;
    struct Node* right;
} Node;

// Traversal in-order secara rekursif.
static inline void archanaberry_crawl(Node* root) {
    if (root == NULL) return;
    archanaberry_crawl(root->left);
    printf("Node value: %d\n", root->value);
    archanaberry_crawl(root->right);
}

// Membuat node tree baru dengan nilai tertentu.
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

// Melepas seluruh tree secara rekursif.
static inline void free_tree(Node* root) {
    if (root == NULL) return;
    free_tree(root->left);
    free_tree(root->right);
    free(root);
}

#endif // BERRYHANDLER_H
