#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include "berryhandler.h"

#define NUM_THREADS 8
#define ITERATIONS 10000

// Global counter beserta mutex untuk proteksi
int global_counter = 0;
pthread_mutex_t counter_mutex = PTHREAD_MUTEX_INITIALIZER;

void* thread_func(void* arg) {
    int thread_id = *(int*)arg;
    
    // Alokasi memori sementara, pastikan selalu dibersihkan
    BERRY_MANUAL_ALLOC(tempMem, 100, thread_id);
    memset(tempMem->ptr, 0, 100);
    
    // Selalu bebaskan memori, tanpa kondisi.
    berry_release(tempMem);
    
    // Update global_counter dengan proteksi mutex untuk menghindari race condition.
    for (int i = 0; i < ITERATIONS; i++) {
        pthread_mutex_lock(&counter_mutex);
        global_counter++;
        pthread_mutex_unlock(&counter_mutex);
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
    
    printf("[FIXED MANUAL] Global counter = %d (seharusnya = %d)\n",
           global_counter, NUM_THREADS * ITERATIONS);
    printf("[FIXED MANUAL] Active allocations = %d\n", berry_active_allocations());
    
    pthread_mutex_destroy(&counter_mutex);
    return 0;
}
