#ifndef ALLOCATOR_H
#define ALLOCATOR_H

#include <stddef.h>
#include <pthread.h>

// Dimensiunea totală a memoriei pe care o gestionăm (ex: 1 MB)
#define MEMORY_SIZE (1024 * 1024)

// Structura unui bloc de memorie
// Aceasta este "meta-data" care stă chiar înainte de datele utilizatorului
typedef struct Block
{
    size_t size;        // Câți octeți are blocul (fără structură)
    int is_free;        // 1 dacă e liber, 0 dacă e ocupat
    struct Block *next; // Pointer către următorul bloc
    struct Block *prev; // Pointer către blocul anterior
} Block;

// Structura generală a memoriei partajate
// Mutex-ul trebuie să stea ÎN memoria partajată ca să fie văzut de toate procesele
typedef struct
{
    pthread_mutex_t mutex;
    Block *start_block;
} SharedMemory;

// Macro pentru a alinia memoria (pentru performanță, opțional dar recomandat)
#define ALIGN(size) (((size) + (sizeof(void *) - 1)) & ~(sizeof(void *) - 1))
#define BLOCK_SIZE ALIGN(sizeof(Block))

// =============================================================
// DECLARAȚII FUNCȚII (API)
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
