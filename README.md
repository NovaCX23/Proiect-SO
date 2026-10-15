# Operating Systems Project - Memory Allocator

This project implements a memory management library (allocator) that can be used concurrently by multiple processes. The library provides functionality similar to `malloc`, `free` and `realloc`, but it works on a shared memory region.

## 👥 Team and Task Split

The project was built by a team of 2 students, with responsibilities split as follows:

### Student 1 (Christian): Infrastructure and Allocation

- **Initialization (`my_init`)**: Setting up the shared memory with `shm_open` + `mmap` and initializing the synchronization mechanisms (mutex).
- **Allocation (`my_malloc`)**: Implementing the **First Fit** algorithm for finding free blocks and the **Split** logic (dividing a large block into two smaller ones).

### Student 2 (Eduard): Management and Optimization

- **Freeing (`my_free`)**: Implementing the logic that marks blocks as free and the **Coalescing** algorithm (merging adjacent free blocks to reduce fragmentation).
- **Reallocation (`my_realloc`)**: Resizing memory blocks (new allocation + copying the data + freeing the old block).

---

## 📂 Project Structure

- `allocator.h` - The header file. Contains the data structure definitions (`Block`, `SharedMemory`) and the function signatures.
- `allocator.c` - The actual library implementation. This is where the logic for managing the doubly linked list of memory blocks lives.
- `tests.c` - Automated tests for the library's functionality (allocation, realloc, coalescing, multi-process).

---

## ⚙️ Technical Details

1.  **Shared Memory**: We use `shm_open` + `mmap` to create a POSIX shared memory object with a fixed name (`/proiect_so_allocator`) that can be accessed from different processes (including after `exec`).
2.  **Synchronization**: To avoid _race conditions_ when several processes request memory at the same time, we use a `pthread_mutex_t` with `PTHREAD_PROCESS_SHARED` and `PTHREAD_MUTEX_ROBUST`. The mutex is stored at the very beginning of the mapped memory region.
3.  **Data Structure**: Memory is managed as a doubly linked list of blocks. Each block has a header (`struct Block`) that holds its size and its state (free/used).

---

## 🚀 How to Build and Run

Because we use threads (for the mutex) and have several source files, the build command is:

```bash
gcc -o tests tests.c allocator.c -pthread
```

Run the tests:

```bash
./tests
```

---

## 🧪 Test Scenarios (in tests.c)

1.  **Simple Allocation Test**: Checks that we can allocate an array of ints and write into it.
2.  **Concurrency Test**: Creates a child process with `fork()`. Both the parent and the child allocate memory at the same time and write data, showing that the mutex correctly protects the structures.
3.  **Concurrency Test (exec)**: The parent starts a child through `exec`, and both processes allocate memory at the same time using the same POSIX shared memory object.
4.  **Coalescing Test**: Allocates 3 consecutive blocks, frees them out of order (b, a, c) and checks that a block as large as all three is placed at the address of the first one, which is only possible if they merged.
5.  **Realloc Test**: Grows a block and checks that the data is preserved.

---

## 🧹 Cleanup (optional)

If you want to manually delete the shared memory object after the tests:

```bash
rm -f /dev/shm/proiect_so_allocator
```
