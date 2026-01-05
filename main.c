#include "allocator.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>

int main()
{
    // 1. Inițializare
    if (my_init() != 0)
    {
        return 1;
    }
    printf("Memorie inițializată. Start teste...\n");

    // 2. Test Simplu (Student 1 & 2 verifică funcționalitatea de bază)
    printf("\n--- Test 1: Alocare simplă ---\n");
    int *v = (int *)my_malloc(10 * sizeof(int));
    if (v)
    {
        for (int i = 0; i < 10; i++)
            v[i] = i;
        printf("Alocat vector, v[9] = %d\n", v[9]);
        my_free(v);
        printf("Eliberat vector.\n");
    }

    // 3. Test Concurență (Cerința principală)
    printf("\n--- Test 2: Concurență (Procese) ---\n");

    pid_t pid = fork();

    if (pid == 0)
    {
        // --- PROCES COPIL ---
        // Student 1: Arată că procesul copil poate aloca
        printf("[Copil] Încearcă să aloce memorie...\n");
        char *s = (char *)my_malloc(100);
        if (s)
        {
            strcpy(s, "Salut din procesul copil!");
            printf("[Copil] Scris în memorie: '%s'\n", s);
            sleep(1); // Simulăm o procesare
            my_free(s);
            printf("[Copil] Memorie eliberată.\n");
        }
        exit(0);
    }
    else if (pid > 0)
    {
        // --- PROCES PĂRINTE ---
        // Student 2: Arată că părintele poate aloca simultan
        printf("[Părinte] Încearcă să aloce memorie...\n");
        char *p = (char *)my_malloc(50);
        if (p)
        {
            strcpy(p, "Salut din părinte!");
            printf("[Părinte] Scris în memorie: '%s'\n", p);
            my_free(p);
        }

        wait(NULL); // Așteptăm copilul
        printf("[Părinte] Copilul a terminat.\n");
    }
    else
    {
        perror("Fork failed");
    }

    // 4. Test Coalescing (Verificare Student 2)
    printf("\n--- Test 3: Coalescing (Unire blocuri) ---\n");
    void *a = my_malloc(100);
    void *b = my_malloc(100);
    void *c = my_malloc(100);

    debug_print_heap(); // Ar trebui să vedem 3 blocuri ocupate

    my_free(b); // Eliberăm mijlocul
    debug_print_heap();

    my_free(a); // Eliberăm stânga -> ar trebui să se unească cu b
    my_free(c); // Eliberăm dreapta -> ar trebui să se unească totul într-un bloc mare

    printf("După eliberare totală:\n");
    debug_print_heap();

    return 0;
}
