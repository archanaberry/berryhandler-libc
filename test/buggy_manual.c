#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include "berryhandler.h"  // Pastikan BerryHandler sudah dikompilasi

#define NUM_THREADS 8
#define ITERATIONS 10000

// Variabel global tanpa proteksi --> race condition
int global_counter = 0;

void* thread_func(void* arg) {
    int thread_id = *(int*)arg;
    
    // Alokasi memori sementara (diallocasi via BerryHandler)
    BERRY_MANUAL_ALLOC(tempMem, 100, thread_id);
    memset(tempMem->ptr, 0, 100);
    
    // BUG INTENSI: Untuk thread dengan ID genap, tidak pernah memanggil berry_release(), sehingga terjadi memory leak.
    if (thread_id % 2 == 0) {
        // Tidak memanggil berry_release(tempMem); -> intentionally leaked
    } else {
        berry_release(tempMem);
    }
    
    // Race condition: update global_counter tanpa lock
    for (int i = 0; i < ITERATIONS; i++) {
        global_counter++;  // Update tanpa proteksi, dapat terjadi kondisi race
    }
    
    return NULL;
}

int main(void) {
    pthread_t threads[NUM_THREADS];
    int thread_ids[NUM_THREADS];
    
    // Buat thread
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_ids[i] = i;
        if (pthread_create(&threads[i], NULL, thread_func, &thread_ids[i]) != 0) {
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
    }
    
    // Tunggu seluruh thread selesai
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    printf("[BUGGY MANUAL] Global counter = %d (harusnya = %d)\n",
           global_counter, NUM_THREADS * ITERATIONS);
    printf("[BUGGY MANUAL] Active allocations = %d\n", berry_active_allocations());
    // Jika BERRY_DEBUG didefinisikan, berry_check_leaks() juga dapat dipanggil.
    
    return 0;
}
