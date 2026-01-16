#include "allocator.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

// ============================================================================
// Global state shared across all allocator functions
// ============================================================================
static void *heap_start = NULL;
static SharedMemory *shm_info = NULL;

// ============================================================================
// Helper conversions for offset-based pointers
// ============================================================================
// Converteste un offset din heap in pointer la Block (NULL pentru OFFSET_NULL)
static Block *offset_to_block(size_t offset)
{
    if (offset == OFFSET_NULL)
        return NULL;
    return (Block *)((char *)heap_start + offset);
}

// Converteste un pointer la Block in offset relativ fata de inceputul heap-ului
static size_t block_to_offset(Block *block)
{
    if (!block)
        return OFFSET_NULL;
    return (size_t)((char *)block - (char *)heap_start);
}

// ============================================================================
// Sincronizare intre procese (mutex robust)
// ============================================================================
// Blocheaza mutex-ul din memoria partajata si il repara daca detinatorul a murit
static int lock_mutex()
{
    int rc = pthread_mutex_lock(&shm_info->mutex);
#ifdef EOWNERDEAD
    if (rc == EOWNERDEAD)
    {
        pthread_mutex_consistent(&shm_info->mutex);
        return 0;
    }
#endif
    return rc;
}

// Initializeaza structurile din memoria partajata (mutex + lista de blocuri)
static void init_shared_memory()
{
    memset(shm_info, 0, sizeof(SharedMemory));

    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
#ifdef PTHREAD_MUTEX_ROBUST
    pthread_mutexattr_setrobust(&attr, PTHREAD_MUTEX_ROBUST);
#endif
    pthread_mutex_init(&shm_info->mutex, &attr);
    pthread_mutexattr_destroy(&attr);

    size_t start_offset = ALIGN(sizeof(SharedMemory));
    shm_info->start_block_offset = start_offset;

    Block *first_block = offset_to_block(start_offset);
    first_block->size = MEMORY_SIZE - start_offset - BLOCK_SIZE;
    first_block->is_free = 1;
    first_block->next_offset = OFFSET_NULL;
    first_block->prev_offset = OFFSET_NULL;

    shm_info->magic = SHM_MAGIC;
    shm_info->initialized = 1;
}

// ============================================================================
// Initializare memorie partajata
// ============================================================================
// Creeaza/atasheaza shared memory si pregateste structurile interne
int my_init()
{
    int fd = shm_open(SHM_NAME, O_RDWR | O_CREAT | O_EXCL, 0660);
    int creator = 1;

    if (fd < 0)
    {
        if (errno != EEXIST)
        {
            perror("Eroare la shm_open");
            return -1;
        }
        creator = 0;
        fd = shm_open(SHM_NAME, O_RDWR, 0660);
        if (fd < 0)
        {
            perror("Eroare la shm_open (existenta)");
            return -1;
        }
    }

    if (creator)
    {
        if (ftruncate(fd, MEMORY_SIZE) != 0)
        {
            perror("Eroare la ftruncate");
            close(fd);
            return -1;
        }
    }

    // Mapam memoria partajata in spatiul procesului
    heap_start = mmap(NULL, MEMORY_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);

    if (heap_start == MAP_FAILED)
    {
        perror("Eroare la mmap");
        return -1;
    }

    shm_info = (SharedMemory *)heap_start;

    // Daca e prima data sau structura e invalida, (re)initializam
    if (creator || shm_info->magic != SHM_MAGIC)
    {
        init_shared_memory();
    }
    else
    {
        while (!shm_info->initialized)
        {
            usleep(1000);
        }
    }

    return 0;
}

// ============================================================================
// Alocare (First Fit + Split)
// ============================================================================
// Aloca un bloc de memorie folosind first-fit si split daca e nevoie
void *my_malloc(size_t size)
{
    if (size == 0 || shm_info == NULL)
        return NULL;

    size_t aligned_size = ALIGN(size);

    if (lock_mutex() != 0)
        return NULL;

    Block *curr = offset_to_block(shm_info->start_block_offset);

    // Cautare first-fit in lista de blocuri
    while (curr != NULL)
    {
        if (curr->is_free && curr->size >= aligned_size)
        {
            // Split: pastram restul ca bloc liber daca incape
            if (curr->size >= aligned_size + BLOCK_SIZE + 8)
            {
                Block *new_block = (Block *)((char *)curr + BLOCK_SIZE + aligned_size);
                size_t new_offset = block_to_offset(new_block);

                new_block->size = curr->size - aligned_size - BLOCK_SIZE;
                new_block->is_free = 1;
                new_block->next_offset = curr->next_offset;
                new_block->prev_offset = block_to_offset(curr);

                if (curr->next_offset != OFFSET_NULL)
                {
                    Block *next_block = offset_to_block(curr->next_offset);
                    next_block->prev_offset = new_offset;
                }
                curr->next_offset = new_offset;
                curr->size = aligned_size;
            }

            curr->is_free = 0;
            pthread_mutex_unlock(&shm_info->mutex);
            return (void *)((char *)curr + BLOCK_SIZE);
        }
        curr = offset_to_block(curr->next_offset);
    }

    pthread_mutex_unlock(&shm_info->mutex);
    return NULL;
}

// ============================================================================
//  Eliberare + Coalescing
// ============================================================================
// Marcheaza blocul ca liber si uneste blocuri libere adiacente
void my_free(void *ptr)
{
    if (!ptr || shm_info == NULL)
        return;

    if (lock_mutex() != 0)
        return;

    Block *curr = (Block *)((char *)ptr - BLOCK_SIZE);
    curr->is_free = 1; // marcam blocul curent liber

    // Coalescing cu urmatorul bloc
    Block *next_block = offset_to_block(curr->next_offset);
    if (next_block != NULL && next_block->is_free)
    {
        curr->size += BLOCK_SIZE + next_block->size;
        curr->next_offset = next_block->next_offset;
        if (curr->next_offset != OFFSET_NULL)
        {
            Block *after_next = offset_to_block(curr->next_offset);
            after_next->prev_offset = block_to_offset(curr);
        }
    }

    // Coalescing cu blocul anterior
    Block *prev_block = offset_to_block(curr->prev_offset);
    if (prev_block != NULL && prev_block->is_free)
    {
        prev_block->size += BLOCK_SIZE + curr->size;
        prev_block->next_offset = curr->next_offset;
        if (curr->next_offset != OFFSET_NULL)
        {
            Block *after_curr = offset_to_block(curr->next_offset);
            after_curr->prev_offset = block_to_offset(prev_block);
        }
    }

    pthread_mutex_unlock(&shm_info->mutex);
}

// ============================================================================
// Realocare (aloca, copiaza, elibereaza)
// ============================================================================
// Redimensioneaza un bloc; muta datele daca e necesar
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
    size_t aligned_size = ALIGN(size);
    // Daca blocul curent e suficient, il pastram
    if (curr->size >= aligned_size)
    {
        return ptr;
    }

    // Alocam un bloc nou si copiem datele vechi
    void *new_ptr = my_malloc(size);
    if (new_ptr)
    {
        memcpy(new_ptr, ptr, curr->size);
        my_free(ptr);
    }
    return new_ptr;
}

// ============================================================================
// Utilitar de debug: afiseaza starea heap-ului
// ============================================================================
// Afiseaza lista de blocuri si starea (liber/ocupat)
void debug_print_heap()
{
    if (!shm_info)
        return;
    if (lock_mutex() != 0)
        return;

    Block *curr = offset_to_block(shm_info->start_block_offset);
    printf("\n[DEBUG] Stare Heap:\n");
    int i = 0;
    while (curr != NULL)
    {
        printf("  Bloc %d: %zu bytes [%s] la adresa %p\n",
               i++, curr->size, curr->is_free ? "LIBER" : "OCUPAT", (void *)curr);
        curr = offset_to_block(curr->next_offset);
    }
    printf("\n");
    pthread_mutex_unlock(&shm_info->mutex);
}
