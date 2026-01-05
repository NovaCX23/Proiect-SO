# Proiect Sisteme de Operare - Allocator de Memorie

Acest proiect implementează o bibliotecă de gestionare a memoriei (alocator) care poate fi utilizată concurent de mai multe procese. Biblioteca oferă funcționalități similare cu `malloc`, `free` și `realloc`, dar lucrează pe o zonă de memorie partajată (Shared Memory).

## 👥 Echipa și Împărțirea Sarcinilor

Proiectul a fost realizat de o echipă de 2 studenți, cu responsabilități împărțite astfel:

### Student 1: Infrastructură și Alocare

- **Inițializare (`my_init`)**: Configurarea memoriei partajate folosind `mmap` și inițializarea mecanismelor de sincronizare (Mutex).
- **Alocare (`my_malloc`)**: Implementarea algoritmului **First Fit** pentru găsirea blocurilor libere și logica de **Split** (împărțirea unui bloc mare în două mai mici).

### Student 2: Gestionare și Optimizare

- **Eliberare (`my_free`)**: Implementarea logicii de marcare a blocurilor ca libere și algoritmul de **Coalescing** (unirea blocurilor libere adiacente pentru a reduce fragmentarea).
- **Realocare (`my_realloc`)**: Redimensionarea blocurilor de memorie (alocare nouă + copiere date + eliberare veche).

---

## 📂 Structura Proiectului

- `allocator.h` - Fișierul header. Conține definițiile structurilor de date (`Block`, `SharedMemory`) și semnăturile funcțiilor.
- `allocator.c` - Implementarea efectivă a bibliotecii. Aici se află logica de gestionare a listei dublu înlănțuite de blocuri de memorie.
- `main.c` - Fișierul de testare. Conține scenarii care demonstrează funcționalitatea bibliotecii (alocare simplă, concurență între procese, coalescing).

---

## ⚙️ Detalii Tehnice

1.  **Memorie Partajată**: Folosim `mmap` cu flag-urile `MAP_SHARED | MAP_ANONYMOUS` pentru a cere memorie de la kernel care să poată fi văzută de procese diferite (ex: după `fork()`).
2.  **Sincronizare**: Pentru a evita _race condition_-urile când mai multe procese cer memorie simultan, folosim un `pthread_mutex_t` setat cu atributul `PTHREAD_PROCESS_SHARED`. Acesta este stocat chiar la începutul zonei de memorie mapate.
3.  **Structura Datelor**: Memoria este gestionată ca o listă dublu înlănțuită de blocuri. Fiecare bloc are un header (`struct Block`) care conține dimensiunea și starea (liber/ocupat).

---

## 🚀 Cum se compilează și rulează

Deoarece folosim thread-uri (pentru mutex) și avem mai multe fișiere sursă, compilarea se face astfel:

```bash
gcc -o proiect main.c allocator.c -pthread
```

Rulare:

```bash
./proiect
```

---

## 🧪 Scenarii de Testare (în main.c)

1.  **Test Alocare Simplă**: Verifică dacă putem aloca și scrie într-un vector de int-uri.
2.  **Test Concurență**: Creează un proces copil cu `fork()`. Atât părintele cât și copilul alocă memorie simultan și scriu date, demonstrând că mutex-ul protejează corect structurile.
3.  **Test Coalescing**: Alocă 3 blocuri și le eliberează în ordine aleatorie pentru a verifica dacă se unesc corect într-un singur bloc mare liber.
