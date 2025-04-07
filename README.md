# berryhandler-libc

Archana Berry Handling Schedule Library for standard libc programs in C with automatic memory handling, lightweight design, high speed, efficiency, effortless coding, anti-memory leak mechanisms, and smart address detection.

---

## Overview

BerryHandler is a lightweight C library designed to bring modern memory management and scheduling features to C programming. It leverages smart allocation with reference counting, a global registry for leak detection, and a robust thread pool scheduler. This library aims to bridge the gap between traditional C memory management and safer, more automated approaches without the overhead of a full garbage collector.

---

## Features

- **Smart Memory Management**  
  - **Reference Counting:** Automatically manage memory through atomic reference counting.  
  - **Global Registry:** Track all allocations with a built-in registry to detect memory leaks.  
  - **Leak Detection:** Provides functions to check for unfreed memory at runtime, helping you catch leaks early.

- **Thread Pool & Scheduler**  
  - **Concurrent Task Execution:** Support for multi-threading with a customizable thread pool (up to `BERRY_MAX_THREADS`).  
  - **Task Management:** Add, cancel, and prioritize tasks within the pool.  
  - **Efficient Synchronization:** Uses mutexes and condition variables for safe and efficient thread communication.

- **Memory & Task Pooling**  
  - **Resource Reuse:** Implements free-list pooling for registry nodes and tasks, reducing dynamic allocation overhead.  
  - **Performance-Oriented:** Designed to minimize allocation delays and fragmentation, ensuring high throughput.

- **Flexible Debug & Production Modes**  
  - **Configurable Logging:** Integrated logging callbacks with a debug mode (`BERRY_DEBUG`) that can be turned off in production for minimal overhead.  
  - **Statistics & Monitoring:** Functions to retrieve active allocation counts and task execution statistics.

- **Extended Utilities**  
  - **Recursive Data Structure Management:** Built-in functions for tree (binary tree) creation, traversal, and cleanup.  
  - **Auto-Cleanup (Optional):** Macro support for automatic cleanup using compiler-specific attributes (if supported).

---

## Getting Started

### Prerequisites

- A C11-compliant compiler (e.g., GCC, Clang) with support for `<stdatomic.h>`.
- POSIX Threads (pthread) library.

### Installation

Simply include `berryhandler.h` in your project. There is no separate build process; the header-only design makes integration straightforward.

### Example Usage

```c
#include "berryhandler.h"

void demo_task(void* arg) {
    int value = *(int*)arg;
    printf("Task running with value: %d on thread: %lu\n", value, pthread_self());
}

int main() {
    // Memory handler usage example
    ArchanaberryMemory *mem = berry_alloc(1024, 1);
    berry_retain(mem);   // Borrow the memory
    berry_return(mem);   // Release one reference
    berry_release(mem);  // Final release, memory freed automatically

    // Thread pool usage example
    BerryThreadPool pool;
    init_thread_pool(&pool, 4);
    start_thread_pool(&pool);
    int task_val = 42;
    add_task(&pool, demo_task, &task_val, 10);
    wait_for_tasks(&pool);
    stop_thread_pool(&pool);

    // Check for memory leaks (debug mode)
    berry_check_leaks();

    return 0;
}
```

---

## Documentation

Detailed API documentation is provided within the header file `berryhandler.h` through inline comments. Refer to the source code for further insights into each function and macro.

---

## License

This project is licensed under the ABPL (Archana Berry Public License) License. See the [ABPL](https://github.com/archanaberry/Lisensi) file for details.

---

## Contributing

Contributions, suggestions, and feedback are welcome! Feel free to open issues or submit pull requests on the project's repository.

---

## Acknowledgments

BerryHandler was inspired by modern memory-safe programming languages like Rust and seeks to bring some of their benefits to C while keeping the lightweight nature and performance of traditional libc programs.
