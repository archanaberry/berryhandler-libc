#define BERRY_AUTOMATIC
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include "berryhandler.h"

#define NUM_THREADS 8
#define ITERATIONS 10000

// Global counter serta mutex untuk proteksi race condition
int global_counter = 0;
pthread_mutex_t counter_mutex = PTHREAD_MUTEX_INITIALIZER;

void* thread_func(void* arg) {
    int thread_id = *(int*)arg;
    
    // Alokasi otomatis; BerryHandler akan memanggil cleanup saat variabel keluar scope.
    BERRY_AUTO_ALLOC(tempMem, 100, thread_id);
    memset(tempMem->ptr, 0, 100);
    
    // Jangan membuat pointer tambahan agar cleanup bekerja dengan benar.
    
    // Update global counter secara aman dengan mutex.
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
    
    printf("[FIXED AUTO] Global counter = %d (seharusnya = %d)\n",
           global_counter, NUM_THREADS * ITERATIONS);
    printf("[FIXED AUTO] Active allocations = %d\n", berry_active_allocations());
    
    pthread_mutex_destroy(&counter_mutex);
    return 0;
}
