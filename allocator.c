#include "allocator.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>

// Variabilă globală (dar va fi mapată în memoria partajată)
static void *heap_start = NULL;
static SharedMemory *shm_info = NULL;

// =============================================================
// IMPLEMENTARE STUDENT 1: Inițializare și Alocare
// =============================================================

/*
   STUDENT 1:
   Această funcție cere memorie de la Kernel folosind mmap.
   Setează mutex-ul să funcționeze între procese (PTHREAD_PROCESS_SHARED).
   Inițializează primul bloc mare gol.
*/
int my_init()
{
    // Cerem memorie de la OS care să fie partajată (MAP_SHARED) și anonimă (fără fișier)
    heap_start = mmap(NULL, MEMORY_SIZE, PROT_READ | PROT_WRITE,
                      MAP_SHARED | MAP_ANONYMOUS, -1, 0);

    if (heap_start == MAP_FAILED)
    {
        perror("Eroare la mmap");
        return -1;
    }

    // Primele date din heap sunt informațiile despre mutex și start
    shm_info = (SharedMemory *)heap_start;

    // Inițializăm atributul mutex-ului pentru a fi partajat între procese
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&shm_info->mutex, &attr);

    // Setăm primul bloc imediat după structura SharedMemory
    void *first_block_addr = (char *)heap_start + sizeof(SharedMemory);
    shm_info->start_block = (Block *)first_block_addr;

    // Configurăm primul bloc: este tot spațiul rămas și este liber
    shm_info->start_block->size = MEMORY_SIZE - sizeof(SharedMemory) - BLOCK_SIZE;
    shm_info->start_block->is_free = 1;
    shm_info->start_block->next = NULL;
    shm_info->start_block->prev = NULL;

    return 0;
}

/*
   STUDENT 1:
   Funcția de alocare.
   1. Blochează mutex-ul.
   2. Caută un bloc liber suficient de mare (First Fit).
   3. Dacă blocul e prea mare, îl sparge în două (Split).
   4. Returnează pointerul către date (după header).
*/
void *my_malloc(size_t size)
{
    if (size <= 0 || shm_info == NULL)
        return NULL;

    size_t aligned_size = ALIGN(size);

    // Blocăm accesul pentru alte procese
    pthread_mutex_lock(&shm_info->mutex);

    Block *curr = shm_info->start_block;

    // Căutăm primul bloc liber (First Fit)
    while (curr != NULL)
    {
        if (curr->is_free && curr->size >= aligned_size)
        {
            // Am găsit un loc!

            // Verificăm dacă putem sparge blocul în două (Split)
            // Dacă rămâne suficient loc pentru încă un header + minim 8 bytes
            if (curr->size >= aligned_size + BLOCK_SIZE + 8)
            {
                Block *new_block = (Block *)((char *)curr + BLOCK_SIZE + aligned_size);

                new_block->size = curr->size - aligned_size - BLOCK_SIZE;
                new_block->is_free = 1;
                new_block->next = curr->next;
                new_block->prev = curr;

                if (curr->next != NULL)
                {
                    curr->next->prev = new_block;
                }
                curr->next = new_block;
                curr->size = aligned_size;
            }

            curr->is_free = 0;
            pthread_mutex_unlock(&shm_info->mutex);

            // Returnăm adresa de după header
            return (void *)((char *)curr + BLOCK_SIZE);
        }
        curr = curr->next;
    }

    // Nu am găsit loc
    pthread_mutex_unlock(&shm_info->mutex);
    return NULL;
}

// =============================================================
// IMPLEMENTARE STUDENT 2: Eliberare și Realocare
// =============================================================

/*
   STUDENT 2:
   Funcția de eliberare.
   1. Calculează adresa header-ului.
   2. Marchează blocul ca liber.
   3. "Coalescing": Verifică dacă vecinii (stânga/dreapta) sunt liberi și îi unește.
*/
void my_free(void *ptr)
{
    if (!ptr)
        return;

    pthread_mutex_lock(&shm_info->mutex);

    // Obținem pointerul către structura Block (mergem în spate cu BLOCK_SIZE)
    Block *curr = (Block *)((char *)ptr - BLOCK_SIZE);
    curr->is_free = 1;

    // Coalescing Dreapta (dacă următorul e liber, îl înghițim)
    if (curr->next != NULL && curr->next->is_free)
    {
        Block *next_block = curr->next;
        curr->size += BLOCK_SIZE + next_block->size;
        curr->next = next_block->next;
        if (curr->next != NULL)
        {
            curr->next->prev = curr;
        }
    }

    // Coalescing Stânga (dacă anteriorul e liber, ne înghite el pe noi)
    if (curr->prev != NULL && curr->prev->is_free)
    {
        Block *prev_block = curr->prev;
        prev_block->size += BLOCK_SIZE + curr->size;
        prev_block->next = curr->next;
        if (curr->next != NULL)
        {
            curr->next->prev = prev_block;
        }
    }

    pthread_mutex_unlock(&shm_info->mutex);
}

/*
   STUDENT 2:
   Funcția de realocare.
   1. Alocă un bloc nou.
   2. Copiază datele.
   3. Eliberează blocul vechi.
*/
void *my_realloc(void *ptr, size_t size)
{
    if (ptr == NULL)
        return my_malloc(size);
    if (size == 0)
    {
        my_free(ptr);
        return NULL;
    }

    Block *curr = (Block *)((char *)ptr - BLOCK_SIZE);
    if (curr->size >= size)
    {
        // Avem deja loc suficient, nu facem nimic (simplificare)
        return ptr;
    }

    void *new_ptr = my_malloc(size);
    if (new_ptr)
    {
        // Copiem datele vechi (minimul dintre dimensiuni)
        memcpy(new_ptr, ptr, curr->size);
        my_free(ptr);
    }
    return new_ptr;
}

void debug_print_heap()
{
    if (!shm_info)
        return;
    pthread_mutex_lock(&shm_info->mutex);
    Block *curr = shm_info->start_block;
    printf("\n[DEBUG] Stare Heap:\n");
    int i = 0;
    while (curr != NULL)
    {
        printf("  Bloc %d: %zu bytes [%s] la adresa %p\n",
               i++, curr->size, curr->is_free ? "LIBER" : "OCUPAT", (void *)curr);
        curr = curr->next;
    }
    printf("\n");
    pthread_mutex_unlock(&shm_info->mutex);
}
