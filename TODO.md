# TODO

Verification legend:
- **[shim]** - checked on Windows with a local harness that replaces `shm_open`/`mmap` with an
  in-memory buffer (single process only, no `fork`). Good for allocator logic, not for IPC.
- **[linux]** - can only be verified on real Linux (WSL2 / a Linux machine). Not run yet.

## Bugs

- [ ] **B1. Mutex is not robust on Linux** - `allocator.c`, `init_shared_memory`.
  Why: `#ifdef PTHREAD_MUTEX_ROBUST` is false on glibc (it is an enum, not a macro), so
  `pthread_mutexattr_setrobust` is never called and a process that dies holding the lock
  blocks everyone forever.
  Verify: [shim] harness reports `setrobust` called 1 time instead of 0;
  [linux] `nm -D tests | grep setrobust` shows the symbol.
- [ ] **B2. `my_init` race between creator and other processes** - `allocator.c`, `my_init`.
  Why: a second process can `mmap` before the creator's `ftruncate` (SIGBUS on first access),
  or see `magic` not yet set and run `init_shared_memory` concurrently with the creator.
  Verify: [linux] new test `test_concurrent_init` (many children call `my_init` at the same
  moment on a fresh object) passes repeatedly; [shim] build + other tests still pass.
- [ ] **B3. `ALIGN` overflow for huge sizes** - `allocator.c`, `my_malloc` / `my_realloc`.
  Why: `ALIGN(SIZE_MAX - 3)` wraps to 0, so `my_malloc` returns a 0-byte block and
  `my_realloc` returns the old pointer unchanged.
  Verify: [shim] new test `test_huge_size` fails before the fix, passes after.
- [ ] **B4. `my_free` accepts double frees and foreign pointers** - `allocator.c`, `my_free`.
  Why: a double free of a block already merged into its predecessor grows the predecessor
  over the next allocated block (overlapping allocations); a pointer outside the heap makes
  `my_free` write into unrelated memory.
  Verify: [shim] new test `test_invalid_free` fails before the fix, passes after.

## Tests

- [ ] **T1. `test_coalescing` does not prove coalescing** - `tests.c`.
  Why: `my_malloc(384)` succeeds from the free tail of the heap even if coalescing is broken.
  Fix: check that the 384-byte block is placed at `a` (only possible if a, b, c merged).
  Verify: [shim] passes on current code, fails when coalescing is disabled on purpose.
- [ ] **T2. Duplicated output after `fork`** - `tests.c`, fork/exec tests.
  Why: the child inherits the parent's unflushed `stdout` buffer and `exit()` prints it again
  when output goes to a file or pipe.
  Verify: [linux] `./tests > out.txt` contains each `[OK]` line once.

## Housekeeping

- [ ] **H1. Compiled `tests` binary is committed, no `.gitignore`** - repo root.
  Why: build output in git gets stale (the committed one predates several commits).
  Verify: `git ls-files` no longer lists `tests`; a fresh build leaves `git status` clean.
