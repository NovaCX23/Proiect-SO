#ifndef ALLOCATOR_H
#define ALLOCATOR_H

#include <stddef.h>
#include <pthread.h>
#include <stdint.h>

// Dimensiunea totală a memoriei pe care o gestionăm (ex: 1 MB)
#define MEMORY_SIZE (1024 * 1024)

// Numele obiectului de memorie partajată (vizibil între procese)
#define SHM_NAME "/proiect_so_allocator"

// Structura unui bloc de memorie
// Aceasta este "meta-data" care stă chiar înainte de datele utilizatorului
// Folosim offset-uri față de începutul heap-ului, ca să fie valide între procese.
typedef struct Block
{
    size_t size;        // Câți octeți are blocul (fără structură)
    int is_free;        // 1 dacă e liber, 0 dacă e ocupat
    size_t next_offset; // Offset către următorul bloc (0 = NULL)
    size_t prev_offset; // Offset către blocul anterior (0 = NULL)
} Block;

// Structura generală a memoriei partajate
// Mutex-ul trebuie să stea ÎN memoria partajată ca să fie văzut de toate procesele
typedef struct
{
    uint32_t magic;
    int initialized;
    pthread_mutex_t mutex;
    size_t start_block_offset;
} SharedMemory;

#define SHM_MAGIC 0x534F414C
#define OFFSET_NULL ((size_t)0)

// Macro pentru a alinia memoria (pentru performanță, opțional dar recomandat)
#define ALIGN(size) (((size) + (sizeof(void *) - 1)) & ~(sizeof(void *) - 1))
#define BLOCK_SIZE ALIGN(sizeof(Block))

// =============================================================
// DECLARAȚII FUNCȚII 
// =============================================================

// Inițializează memoria partajată și mutex-ul
int my_init();

// Alocă un bloc de memorie de dimensiunea specificată
void *my_malloc(size_t size);

// Eliberează un bloc de memorie
void my_free(void *ptr);

// Redimensionează un bloc de memorie
void *my_realloc(void *ptr, size_t size);

// Funcție de debug pentru a afișa starea heap-ului
void debug_print_heap();

#endif
