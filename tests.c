#include "allocator.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

    my_free(b);
    my_free(a);
    my_free(c);

    void *big = my_malloc(384);
    assert_true(big != NULL, "coalescing creates a large block");
    if (big)
        my_free(big);
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
    test_multi_process_fork();
    test_multi_process_exec(argv[0]);

    if (failures == 0)
    {
        printf("\nAll tests passed.\n");
        return 0;
    }

    printf("\nTests failed: %d\n", failures);
    return 1;
}
