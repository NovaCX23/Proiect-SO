# Bugs

## Found

### B4. `my_free` accepts double frees and pointers it never returned
`allocator.c`, `my_free`.

Reproduction (double free):
```c
a = my_malloc(64); b = my_malloc(64); c = my_malloc(64); d = my_malloc(64);
my_free(a); my_free(b);   /* b is merged into a (160 bytes) */
my_free(b);               /* stale header of b: merges into a AGAIN -> a grows to 256 */
p = my_malloc(200);       /* returns a, which now overlaps c */
```
Reproduction (foreign pointer): `char buf[256] = {0}; my_free(buf + 128);` writes
`is_free = 1` into `buf`.

## Fixed

### B1. Mutex is not robust on Linux - commit "fix: always make the shared mutex robust"
`allocator.c`, `init_shared_memory`: `pthread_mutexattr_setrobust` is guarded by
`#ifdef PTHREAD_MUTEX_ROBUST`. In glibc `PTHREAD_MUTEX_ROBUST` is an enum constant, not a
macro, so the call is compiled out and the mutex is a normal (non-robust) one.

Reproduction: the committed Linux binary `tests` (built from `786d3f4`, same `#ifdef`) imports
`pthread_mutex_consistent` but not `pthread_mutexattr_setrobust` (`nm -D tests`). If a process
is killed while inside `my_malloc`/`my_free`, every other process blocks forever in
`pthread_mutex_lock`, and the `EOWNERDEAD` branch in `lock_mutex` is dead code.

Fix: call `pthread_mutexattr_setrobust` unconditionally (POSIX.1-2008, glibc >= 2.12).
No automated test: the public API cannot hold the lock and die on purpose.
Verified on the Windows shim (call count 0 -> 1); on Linux check with `nm -D tests`.

### B2. `my_init` race between the creator and other processes - commit "fix: make non-creator processes wait for my_init instead of re-initializing"
`allocator.c`, `my_init`:
1. The creator does `shm_open(O_CREAT | O_EXCL)` and only then `ftruncate`. Another process
   that opens the object in between maps a 0-byte object; reading `shm_info->magic` -> SIGBUS.
   If the creator dies in that window, every later `my_init` gets SIGBUS until the object is
   removed by hand.
2. A non-creator that reads `magic != SHM_MAGIC` (creator still initializing) calls
   `init_shared_memory` itself, which `memset`s the mutex and the block list while the creator
   (or the creator's first `my_malloc`) is using them.

Reproduction: start several processes that call `my_init` at the same moment on a fresh object
(`rm /dev/shm/proiect_so_allocator` first). The existing tests never hit this because the
parent always calls `my_init` before `fork`/`exec`. Test: `test_concurrent_init`.

Fix: only the creator initializes. A non-creator waits (max ~5 s) until `fstat` shows the full
size, then until `initialized == 1` (release store / acquire load); on timeout `my_init` prints
an error and returns -1. Behaviour change: an object left half-initialized by a crashed creator
is no longer silently re-initialized; it must be removed with
`rm /dev/shm/proiect_so_allocator` (the error message says so).
Verified on the Windows shim: build clean, single-process tests pass, a 0-byte leftover object
gives an error instead of SIGBUS. **Not yet verified on Linux** (`test_concurrent_init`).

### B3. `ALIGN` overflow for huge sizes - commit "fix: reject sizes larger than the heap before aligning"
`allocator.c`, `my_malloc` / `my_realloc`: `ALIGN(SIZE_MAX - 3)` wraps around to 0.

Reproduction: `my_malloc((size_t)-4)` returns a non-NULL pointer to a 0-byte block;
`my_realloc(p, (size_t)-4)` returns `p` unchanged. Writing to either corrupts the heap.
Test: `test_huge_size`.

Fix: return NULL when `size > MEMORY_SIZE` before calling `ALIGN`; `my_realloc` keeps the old
block, like the standard `realloc`. Verified on the Windows shim: test fails before, passes after.

## Suggestions

(none yet)
