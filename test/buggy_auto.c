#define BERRY_AUTOMATIC
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include "berryhandler.h"

#define NUM_THREADS 8
#define ITERATIONS 10000

// Global counter tanpa proteksi
int global_counter = 0;

void* thread_func(void* arg) {
    int thread_id = *(int*)arg;
    
    // Alokasi otomatis, cleanup terjadi saat variabel keluar scope.
    BERRY_AUTO_ALLOC(tempMem, 100, thread_id);
    memset(tempMem->ptr, 0, 100);
    
    // BUG INTENSI: Simulasikan kebocoran dengan "menggandakan" pointer
    // sehingga cleanup hanya terjadi satu kali.
    if (thread_id % 2 == 0) {
        // Buat pointer tambahan yang mereferensikan memory,
        // lalu hilangkan referensinya sehingga cleanup otomatis tidak menyentuhnya.
        void* leak = tempMem->ptr;
        (void)leak;  // tidak pernah dibebaskan karena cleanup hanya berlaku untuk tempMem.
    }
    
    // Update global counter tanpa perlindungan (race condition)
    for (int i = 0; i < ITERATIONS; i++) {
        global_counter++;
    }
    
    return NULL;
}

int main(void) {
    pthread_t threads[NUM_THREADS];
    int thread_ids[NUM_THREADS];
    
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_ids[i] = i;
        if (pthread_create(&threads[i], NULL, thread_func, &thread_ids[i]) != 0) {
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
    }
    
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    printf("[BUGGY AUTO] Global counter = %d (harusnya = %d)\n",
           global_counter, NUM_THREADS * ITERATIONS);
    printf("[BUGGY AUTO] Active allocations = %d\n", berry_active_allocations());
    return 0;
}
