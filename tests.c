#include "allocator.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

static int failures = 0;

static void assert_true(int cond, const char *msg)
{
    if (!cond)
    {
        failures++;
        printf("[FAIL] %s\n", msg);
    }
    else
    {
        printf("[OK]   %s\n", msg);
    }
}

static void test_basic_alloc_free()
{
    printf("\n--- Test: basic alloc/free ---\n");
    int *v = (int *)my_malloc(10 * sizeof(int));
    assert_true(v != NULL, "my_malloc returns non-NULL");
    if (v)
    {
        for (int i = 0; i < 10; i++)
            v[i] = i * 2;
        assert_true(v[9] == 18, "write/read inside allocated block");
        my_free(v);
    }
}

static void test_realloc_grow()
{
    printf("\n--- Test: realloc grow ---\n");
    char *p = (char *)my_malloc(8);
    assert_true(p != NULL, "initial alloc for realloc");
    if (!p)
        return;

    strcpy(p, "ABC");
    p = (char *)my_realloc(p, 64);
    assert_true(p != NULL, "realloc returns non-NULL");
    if (p)
    {
        assert_true(strcmp(p, "ABC") == 0, "realloc preserves old data");
        strcat(p, "DEF");
        assert_true(strcmp(p, "ABCDEF") == 0, "realloc allows growth");
        my_free(p);
    }
}

static void test_coalescing()
{
    printf("\n--- Test: coalescing ---\n");
    void *a = my_malloc(128);
    void *b = my_malloc(128);
    void *c = my_malloc(128);

    assert_true(a && b && c, "allocated three blocks");

    // a, b, c consecutive: 384 octeti incap la adresa lui a doar daca s-au unit
    int contiguous = (char *)b == (char *)a + 128 + BLOCK_SIZE &&
                     (char *)c == (char *)b + 128 + BLOCK_SIZE;

    my_free(b);
    my_free(a);
    my_free(c);

    void *big = my_malloc(384);
    assert_true(big != NULL, "coalescing creates a large block");
    if (contiguous)
        assert_true(big == a, "merged block is reused at the address of a");
    else
        printf("[SKIP] blocks not contiguous, cannot check the merge address\n");
    if (big)
        my_free(big);
}

static void test_huge_size()
{
    printf("\n--- Test: huge size ---\n");
    // SIZE_MAX - 3: ALIGN ar da overflow la 0
    void *p = my_malloc((size_t)-4);
    assert_true(p == NULL, "my_malloc(SIZE_MAX - 3) returns NULL");
    if (p)
        my_free(p);

    char *q = (char *)my_malloc(16);
    assert_true(q != NULL, "alloc before huge realloc");
    if (!q)
        return;
    strcpy(q, "data");
    char *r = (char *)my_realloc(q, (size_t)-4);
    assert_true(r == NULL, "my_realloc(p, SIZE_MAX - 3) returns NULL");
    assert_true(strcmp(q, "data") == 0, "old block is kept after failed realloc");
    my_free(r ? r : q);
}

static void test_invalid_free()
{
    printf("\n--- Test: double free / invalid pointer ---\n");
    char *a = (char *)my_malloc(64);
    char *b = (char *)my_malloc(64);
    char *c = (char *)my_malloc(64);
    char *d = (char *)my_malloc(64);
    assert_true(a && b && c && d, "allocated four blocks");
    if (!(a && b && c && d))
        return;

    my_free(a);
    my_free(b); // b se uneste cu a
    my_free(b); // double free: nu trebuie sa mareasca a peste c

    char *p = (char *)my_malloc(200);
    int overlap = p && p < c + 64 && c - BLOCK_SIZE < p + 200;
    assert_true(p != NULL && !overlap, "double free does not create overlapping blocks");
    if (p)
        my_free(p);
    my_free(c);
    my_free(d);

    char outside[256] = {0};
    my_free(outside + 128);
    int untouched = 1;
    for (int i = 0; i < 256; i++)
        if (outside[i] != 0)
            untouched = 0;
    assert_true(untouched, "my_free ignores pointers outside the heap");
}

static void test_multi_process_fork()
{
    printf("\n--- Test: multi-process fork ---\n");
    pid_t pid = fork();
    if (pid == 0)
    {
        char *child = (char *)my_malloc(64);
        if (child)
        {
            strcpy(child, "child");
            my_free(child);
            exit(0);
        }
        exit(1);
    }
    else if (pid > 0)
    {
        char *parent = (char *)my_malloc(64);
        assert_true(parent != NULL, "parent can allocate");
        if (parent)
        {
            strcpy(parent, "parent");
            my_free(parent);
        }
        int status = 0;
        waitpid(pid, &status, 0);
        assert_true(WIFEXITED(status) && WEXITSTATUS(status) == 0, "child exited cleanly");
    }
    else
    {
        assert_true(0, "fork failed");
    }
}

static void test_multi_process_exec(char *self_path)
{
    printf("\n--- Test: multi-process exec ---\n");
    pid_t pid = fork();
    if (pid == 0)
    {
        execl(self_path, self_path, "child", NULL);
        exit(1);
    }
    else if (pid > 0)
    {
        char *parent = (char *)my_malloc(64);
        assert_true(parent != NULL, "parent can allocate (exec test)");
        if (parent)
        {
            strcpy(parent, "parent_exec");
            my_free(parent);
        }
        int status = 0;
        waitpid(pid, &status, 0);
        assert_true(WIFEXITED(status) && WEXITSTATUS(status) == 0, "exec child exited cleanly");
    }
    else
    {
        assert_true(0, "fork failed (exec test)");
    }
}

// Mai multe procese apeleaza my_init in acelasi moment pe un obiect nou.
// Sterge obiectul de memorie partajata, deci trebuie rulat ultimul.
static void test_concurrent_init()
{
    printf("\n--- Test: concurrent my_init ---\n");
    int ok = 1;
    for (int round = 0; round < 20 && ok; round++)
    {
        shm_unlink(SHM_NAME);
        int barrier[2];
        if (pipe(barrier) != 0)
        {
            assert_true(0, "pipe failed");
            return;
        }
        pid_t pids[8];
        int n = 0;
        fflush(stdout);
        for (int i = 0; i < 8; i++)
        {
            pid_t pid = fork();
            if (pid == 0)
            {
                char c;
                alarm(10); // un deadlock devine eroare, nu blocheaza testele
                close(barrier[1]);
                if (read(barrier[0], &c, 1) < 0) // asteptam pana parintele inchide pipe-ul
                    _exit(1);
                if (my_init() != 0)
                    _exit(1);
                char *p = (char *)my_malloc(64);
                if (!p)
                    _exit(1);
                strcpy(p, "init");
                my_free(p);
                _exit(0);
            }
            if (pid > 0)
                pids[n++] = pid;
        }
        close(barrier[0]);
        close(barrier[1]); // toti copiii pornesc deodata
        for (int i = 0; i < n; i++)
        {
            int status = 0;
            waitpid(pids[i], &status, 0);
            if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
                ok = 0;
        }
        if (n != 8)
            ok = 0;
    }
    assert_true(ok, "processes calling my_init at the same time all succeed");
}

int main(int argc, char **argv)
{
    if (my_init() != 0)
    {
        printf("[FAIL] my_init failed\n");
        return 1;
    }

    if (argc > 1 && strcmp(argv[1], "child") == 0)
    {
        char *child = (char *)my_malloc(64);
        if (child)
        {
            strcpy(child, "child_exec");
            my_free(child);
            return 0;
        }
        return 1;
    }

    test_basic_alloc_free();
    test_realloc_grow();
    test_coalescing();
    test_huge_size();
    test_invalid_free();
    test_multi_process_fork();
    test_multi_process_exec(argv[0]);
    test_concurrent_init();

    if (failures == 0)
    {
        printf("\nAll tests passed.\n");
        return 0;
    }

    printf("\nTests failed: %d\n", failures);
    return 1;
}
