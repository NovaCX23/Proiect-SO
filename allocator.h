#ifndef ALLOCATOR_H
#define ALLOCATOR_H

#include <stddef.h>
#include <pthread.h>
#include <stdint.h>

// Dimensiunea totala a memoriei pe care o gestionam (ex: 1 MB)
#define MEMORY_SIZE (1024 * 1024)

// Numele obiectului de memorie partajata (vizibil intre procese)
#define SHM_NAME "/proiect_so_allocator"

// Structura unui bloc de memorie
// Aceasta este "meta-data" care sta chiar inainte de datele utilizatorului
// Folosim offset-uri fata de inceputul heap-ului, ca sa fie valide intre procese.
typedef struct Block
{
    size_t size;        // Cati octeti are blocul
    int is_free;        // 1 daca e liber, 0 daca e ocupat
    size_t next_offset; // Offset catre urmatorul bloc (0 = NULL)
    size_t prev_offset; // Offset catre blocul anterior (0 = NULL)
} Block;

// Structura generala a memoriei partajate
// Mutex-ul trebuie sa stea IN memoria partajata ca sa fie vazut de toate procesele
typedef struct
{
    uint32_t magic;
    int initialized;
    pthread_mutex_t mutex;
    size_t start_block_offset;
} SharedMemory;

#define SHM_MAGIC 0x534F414C
#define OFFSET_NULL ((size_t)0)

// Macro pentru a alinia memoria (pentru performanta)
#define ALIGN(size) (((size) + (sizeof(void *) - 1)) & ~(sizeof(void *) - 1))
#define BLOCK_SIZE ALIGN(sizeof(Block))

// =============================================================
// DECLARATII FUNCTII 
// =============================================================

// Initializeaza memoria partajata si mutex-ul
int my_init();

// Alocă un bloc de memorie de dimensiunea specificata
void *my_malloc(size_t size);

// Elibereaza un bloc de memorie
void my_free(void *ptr);

// Redimensioneaza un bloc de memorie
void *my_realloc(void *ptr, size_t size);

// Functie de debug pentru a afisa starea heap-ului
void debug_print_heap();

#endif
