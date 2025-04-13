# BerryHandler-libc

**Archana Berry Memory & Scheduling Library** for standard libc programs in C, offering automatic memory handling, a lightweight design, high speed and efficiency, effortless coding, built-in anti-memory leak mechanisms, and smart address detection.

---

## Overview

BerryHandler is a lightweight C library designed to bridge the gap between traditional manual memory management and modern automated practices. It offers features such as smart memory allocation with reference counting, a global registry to catch leaks, and a robust thread pool scheduler that supports both automatic and manual thread pool management. BerryHandler allows C developers to write safer and more efficient programs while still maintaining the performance benefits of a low-level language.

---

## Features

### Smart Memory Management

- **Reference Counting:** Automatically manage memory using atomic reference counting.
- **Global Registry:** Keep track of all allocations to help detect memory leaks.
- **Leak Detection:** Provides runtime functions to verify that all allocated memory is properly released.

### Thread Pool and Scheduler

- **Concurrent Task Execution:** Supports multi-threading with a customizable thread pool (up to `BERRY_MAX_THREADS`).
- **Task Management:** Easily add, cancel, and prioritize tasks.
- **Efficient Synchronization:** Uses mutexes and condition variables for safe, efficient thread communication.

### Memory & Task Pooling

- **Resource Reuse:** Implements free-list pooling for registry nodes and tasks to reduce dynamic allocation overhead.
- **Performance-Oriented:** Minimizes delays and fragmentation, ensuring high throughput.

### Flexible Debug and Production Modes

- **Configurable Logging:** Integrated logging callbacks with a debug mode (`BERRY_DEBUG`) that can be disabled in production.
- **Statistics & Monitoring:** Get real-time information about active allocations and task statistics.

### Extended Utilities

- **Recursive Data Structure Management:** Includes helper functions for tree creation, traversal, and cleanup.
- **Auto-Cleanup (Optional):** Support for automatic cleanup via compiler-specific attributes if available.

---

## Getting Started

### Prerequisites

- A C11-compliant compiler (e.g., GCC, Clang) with support for `<stdatomic.h>`.
- POSIX Threads (`pthread`) library.

### Installation

Since BerryHandler is a header-only library, simply include `berryhandler.h` in your project. There is no separate build process.

### Example Usage

Below are example programs demonstrating different usage styles. Note that `test2.c` is omitted (assumed to be a typo), so the examples have been renumbered accordingly.

#### test0.c – Automatic Memory Allocation with Automatic Thread Pool Run

```c
#include "berryhandler.h"
#include <string.h>
#include <stdio.h>

void task_loop(void *arg) {
    volatile unsigned long long *counter = (unsigned long long *)arg;
    // Perform 1 billion iterations
    for (unsigned long long i = 0; i < 1000000000ULL; i++) {
        (*counter)++;
    }
}

int main(void) {
    // Automatic memory allocation
    BERRY_AUTO_ALLOC(mem, 1024);
    memset(mem->ptr, 0, mem->size);

    volatile unsigned long long counter = 0;

    // Use the automatic thread pool.
    // For example, specify the number of threads (here, 8 threads).
    ARCHANABERRY_AUTO_RUN(8, {
        // Add task to thread pool
        add_task(&pool, task_loop, (void *)&counter, 10);
    });

    // Print the counter (should be 1 billion if only one task is executed)
    printf("Counter: %llu\n", counter);
    return 0;
}
```

#### test1.c – Manual Memory Allocation and Thread Pool Initialization

```c
#include "berryhandler.h"
#include <string.h>
#include <stdio.h>

void task_loop(void *arg) {
    volatile unsigned long long *counter = (unsigned long long *)arg;
    // Perform 1 billion iterations
    for (unsigned long long i = 0; i < 1000000000ULL; i++) {
        (*counter)++;
    }
}

int main(void) {
    // Manual memory allocation
    BERRY_MANUAL_ALLOC(mem, 2048);
    memset(mem->ptr, 0, mem->size);

    volatile unsigned long long counter = 0;
    
    // Initialize thread pool manually
    BerryThreadPool pool;
    // For example, use 8 threads (you can adjust as needed)
    archanaberry_start(&pool, 8);
    
    // Add the task to the thread pool
    add_task(&pool, task_loop, (void *)&counter, 10);
    
    // Wait for tasks to complete and stop the thread pool
    archanaberry_finish(&pool);
    
    // Manually free allocated memory
    berry_release(mem);
    
    printf("Counter: %llu\n", counter);
    return 0;
}
```

#### test2.c – Manual Task Scheduling with Timing

```c
// test_manual.c - Manual Version
#include "berryhandler.h"
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <string.h>

#define LOOP_COUNT 1000000000ULL

void task_loop(void *arg) {
    volatile unsigned long long *sum = (volatile unsigned long long *)arg;
    for (unsigned long long i = 0; i < LOOP_COUNT; i++) {
        *sum += i % 7;
    }
}

int main(void) {
    BERRY_MANUAL_ALLOC(mem, 2048);
    memset(mem->ptr, 0, mem->size);

    volatile unsigned long long sum = 0;
    BerryThreadPool pool;

    clock_t start = clock();
    archanaberry_start(&pool, 8);
    add_task(&pool, task_loop, (void *)&sum, 10);
    archanaberry_finish(&pool);
    clock_t end = clock();
    double duration = (double)(end - start) / CLOCKS_PER_SEC;
    
    printf("Loop finished. Sum: %llu\n", sum);
    printf("Execution time (manual): %.3f seconds\n", duration);

    berry_release(mem);
    return 0;
}
```

#### test3.c – Automatic Task Scheduling with Timing

```c
// test_auto.c - Automatic Version
#include "berryhandler.h"
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <string.h>

#define LOOP_COUNT 1000000000ULL

void task_loop(void *arg) {
    volatile unsigned long long *sum = (volatile unsigned long long *)arg;
    for (unsigned long long i = 0; i < LOOP_COUNT; i++) {
        *sum += i % 7;
    }
}

int main(void) {
    BERRY_AUTO_ALLOC(mem, 1024);
    memset(mem->ptr, 0, mem->size);

    volatile unsigned long long sum = 0;
    
    clock_t start = clock();
    ARCHANABERRY_AUTO_RUN(8, {
        add_task(&pool, task_loop, (void *)&sum, 10);
    });
    clock_t end = clock();
    double duration = (double)(end - start) / CLOCKS_PER_SEC;

    printf("Loop finished. Sum: %llu\n", sum);
    printf("Execution time (multi-thread auto): %.3f seconds\n", duration);
    
    return 0;
}
```

#### test4.c – Multi-Threaded Automatic Version (With Work Partitioning)

```c
#include "berryhandler.h"
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <string.h>

#define LOOP_COUNT 1000000000ULL  // 1 Billion
#define THREAD_COUNT 8            // Number of threads

typedef struct {
    uint64_t start;
    uint64_t end;
    volatile uint64_t *result;
} ThreadArg;

void task_loop(void *arg) {
    ThreadArg *targ = (ThreadArg *)arg;
    uint64_t local_sum = 0;
    
    for (uint64_t i = targ->start; i < targ->end; ++i) {
        local_sum += i % 7;
    }
    
    // Atomically add the local sum to the global result
    __sync_fetch_and_add(targ->result, local_sum);
}

int main(void) {
    // Automatic memory allocation
    BERRY_AUTO_ALLOC(mem, 2048);
    memset(mem->ptr, 0, mem->size);

    volatile uint64_t sum = 0;
    ThreadArg thread_args[THREAD_COUNT];
    uint64_t chunk = LOOP_COUNT / THREAD_COUNT;
    
    clock_t start = clock();
    ARCHANABERRY_AUTO_RUN(THREAD_COUNT, {
        for (int i = 0; i < THREAD_COUNT; ++i) {
            thread_args[i].start  = i * chunk;
            thread_args[i].end    = (i == THREAD_COUNT - 1) ? LOOP_COUNT : (i + 1) * chunk;
            thread_args[i].result = &sum;
            add_task(&pool, task_loop, &thread_args[i], 10);
        }
    });
    clock_t end = clock();
    double duration = (double)(end - start) / CLOCKS_PER_SEC;
    
    printf("Loop finished. Sum: %llu\n", (unsigned long long)sum);
    printf("Execution time (multi-thread auto): %.3f seconds\n", duration);
    
    return 0;
}
```

#### test5.c – Manual Thread Pool with Parallel Task Scheduling

```c
#include "berryhandler.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#define LOOP_COUNT 1000000000ULL  // 1 Billion
#define THREAD_COUNT 8            // Number of threads

typedef struct {
    uint64_t start;
    uint64_t end;
    volatile uint64_t *result;
} ThreadArg;

void task_loop(void *arg) {
    ThreadArg *targ = (ThreadArg *)arg;
    uint64_t local_sum = 0;
    
    for (uint64_t i = targ->start; i < targ->end; ++i) {
        local_sum += i % 7;
    }
    
    __sync_fetch_and_add(targ->result, local_sum);
}

int main(void) {
    // Manual memory allocation
    BERRY_MANUAL_ALLOC(mem, 2048);
    memset(mem->ptr, 0, mem->size);

    BerryThreadPool pool;
    volatile uint64_t sum = 0;
    ThreadArg thread_args[THREAD_COUNT];
    uint64_t chunk = LOOP_COUNT / THREAD_COUNT;
    
    // Initialize thread pool manually
    archanaberry_start(&pool, THREAD_COUNT);
    
    // Start timing
    clock_t start = clock();
    
    // Add tasks with partitioned work
    for (int i = 0; i < THREAD_COUNT; ++i) {
        thread_args[i].start  = i * chunk;
        thread_args[i].end    = (i == THREAD_COUNT - 1) ? LOOP_COUNT : (i + 1) * chunk;
        thread_args[i].result = &sum;
        add_task(&pool, task_loop, &thread_args[i], 10);
    }
    
    // Wait until all threads have finished execution
    archanaberry_finish(&pool);
    
    // End timing
    clock_t end = clock();
    double duration = (double)(end - start) / CLOCKS_PER_SEC;
    
    printf("Loop finished. Sum: %llu\n", (unsigned long long)sum);
    printf("Execution time (manual + parallel): %.3f seconds\n", duration);
    
    berry_release(mem);
    return 0;
}
```

---

## Benchmark Comparison

Here is a sample benchmark output comparison using a traditional C test (`ctest`) versus the BerryHandler examples:

## Benchmark Results

| Program     | Description                        | Measured Time (in-code) | `real` | `user` | `sys` |
|-------------|------------------------------------|--------------------------|--------|--------|-------|
| `./ctest`   | Loop sum (manual implementation)   | 3.240s                   | 3.259s | 3.247s | 0.005s |
| `./test0`   | Simple counter to 1 billion        | -                        | 2.598s | 2.566s | 0.010s |
| `./test1`   | Simple counter to 1 billion        | -                        | 2.596s | 2.566s | 0.008s |
| `./test2`   | Loop sum (with manual timing)      | 3.489s                   | 3.536s | 3.489s | 0.012s |
| `./test3`   | Loop sum (multi-threaded)          | 3.512s                   | 3.535s | 3.508s | 0.016s |
| `./test4`   | Loop sum (multi-threaded auto)     | 4.292s                   | 0.686s | 4.294s | 0.009s |
| `./test5`   | Manual + parallel implementation   | 4.327s                   | 0.638s | 4.324s | 0.017s |

> **Notes**:  
- `real`: Total elapsed wall-clock time.  
- `user`: Time spent in user-mode (CPU computation).  
- `sys`: Time spent in kernel-mode (e.g., I/O operations).  
- Programs with low `real` time but high `user` time (like `test5`, `test6`) indicate heavy parallel processing (multi-threaded execution).

### Insights

- **Real vs. CPU Time:** In the standard test (ctest), a real time of ~3.24 seconds with the CPU time summing to ~4.3 seconds (user+sys) suggests good multicore utilization.
- **Single vs. Parallel:** Tests 0 and 1 show similar performance for single-thread tasks. Meanwhile, tests 5 and 6 demonstrate true parallelism, partitioning the work among threads.
- **Hybrid Modes:** BerryHandler gives you the flexibility of choosing between automatic and manual modes for thread management—allowing for simple usage or full control over parallel execution.

---

## Documentation

The complete API documentation is provided directly in the header file (`berryhandler.h`) via inline comments. For detailed information about each function, macro, and usage mode, please refer to the source code.

---

## License

This project is licensed under the **Archana Berry Public License (ABPL)**. See the included ABPL file for further details.

---

## Contributing

Contributions, suggestions, and improvements are very welcome! Please open issues or submit pull requests on the repository.

---

## Acknowledgments

BerryHandler was inspired by modern memory-safe programming languages like Rust. It aims to bring some of those benefits to C while retaining the lightweight nature and performance of standard libc programs.
