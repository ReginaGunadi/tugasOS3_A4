#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h> // NOTE: ini kalo merah, berarti karena lagi di OS Windows
                    // nanti kalo di-run di terminal Linux, akan aman. (Wait lagi ngecek)
#include "banner.h" // Panggil modul banner
#include "logger.h"

// Fungsi pembaca metrik VmSize & VmRSS dari /proc/self/status
long get_vmsize_kb() {
    // Buka file status proses sendiri buat ngambil ukuran memori virtual
    FILE *file = fopen("/proc/self/status", "r");
    if (!file) return -1;
   
    char line[128];
    long vmsize = 0;
   
    // Cari baris yang tulisannya "VmSize:"
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "VmSize:", 7) == 0) {
            sscanf(line + 7, "%ld", &vmsize);
            break;
        }
    }
    fclose(file);
    return vmsize;
}

long get_vmrss_kb() {
    // Buka file status proses sendiri buat ngambil ukuran memori fisik (RAM)
    FILE *file = fopen("/proc/self/status", "r");
    if (!file) return -1;
   
    char line[128];
    long vmrss = 0;
   
    // Cari baris yang tulisannya "VmRSS:"
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "VmRSS:", 6) == 0) {
            sscanf(line + 6, "%ld", &vmrss);
            break;
        }
    }
    fclose(file);
    return vmrss;
}

void alokasi_malloc() {
    printf("\nYou picked malloc/free.\n");

    // catat VmSize dan VmRSS
    long vmsize_before = get_vmsize_kb();
    long vmrss_before = get_vmrss_kb();
    printf("VmSize Before: %ld KB | VmRSS Before: %ld KB\n", vmsize_before, vmrss_before);

    long long n;
    printf("How many integers to allocate: ");
    if (scanf("%lld", &n) != 1 || n <= 0) {
        printf("Input jumlahnya ngaco bro!\n\n");
        while (getchar() != '\n');
        return;
    }

    // minta memori ke heap pake malloc
    int *arr = (int *)malloc(n * sizeof(int));
    if (arr == NULL) {
        perror("Waduh, malloc-nya ambruk (gagal)!");
        return;
    }

    // cetak alamat memori awalnya dalam bentuk heksadesimal
    printf("Starting memory address: %p\n", (void *)arr);

    // isi array-nya biar halamannya kesentuh (valuenya 1 sampe n)
    for (long long i = 0; i < n; i++) {
        arr[i] = (int)(i + 1);
    }

    // cetak 10 nilai pertama secara reverse (dari belakang ke depan)
    printf("Printing first 10 values in reverse order:\n");
    long long limit = n < 10 ? n : 10;
    for (long long i = 0; i < limit; i++) {
        printf("%d\n", arr[n - 1 - i]);
    }

    // catat lagi VmSize & VmRSS sesudah memori dipake
    long vmsize_after = get_vmsize_kb();
    long vmrss_after = get_vmrss_kb();
    printf("VmSize After: %ld KB | VmRSS After: %ld KB\n\n", vmsize_after, vmrss_after);

    if (log_allocation("malloc", (size_t)n, arr, arr,
                       vmsize_before, vmrss_before, vmsize_after, vmrss_after) != 0) {
        fprintf(stderr, "Peringatan: pencatatan log malloc tidak lengkap.\n");
    }

    // jangan lupa dibebasin pake free biar gak memory leak
    free(arr);
}

void alokasi_mmap() {
    printf("\nYou picked mmap/munmap.\n");

    // catat metrik sebelum mmap dipanggil
    long vmsize_before = get_vmsize_kb();
    long vmrss_before = get_vmrss_kb();
    printf("VmSize Before: %ld KB | VmRSS Before: %ld KB\n", vmsize_before, vmrss_before);

    long long n;
    printf("How many integers to allocate: ");
    if (scanf("%lld", &n) != 1 || n <= 0) {
        printf("Input jumlahnya ngaco bro!\n\n");
        while (getchar() != '\n');
        return;
    }

    size_t bytes = n * sizeof(int);

    // minta memori langsung ke kernel pake mmap anonymous
    int *arr = (int *)mmap(NULL, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (arr == MAP_FAILED) {
        perror("Waduh, mmap-nya gagal total!");
        return;
    }

    // cetak alamat memori awal hasil mmap
    printf("Starting memory address: %p\n", (void *)arr);

    // isi array-nya
    for (long long i = 0; i < n; i++) {
        arr[i] = (int)(i + 1);
    }

    // cetak 10 nilai pertama secara reverse
    printf("Printing first 10 values in reverse order:\n");
    long long limit = n < 10 ? n : 10;
    for (long long i = 0; i < limit; i++) {
        printf("%d\n", arr[n - 1 - i]);
    }

    // catat metrik sesudah mmap dipake
    long vmsize_after = get_vmsize_kb();
    long vmrss_after = get_vmrss_kb();
    printf("VmSize After: %ld KB | VmRSS After: %ld KB\n\n", vmsize_after, vmrss_after);

    if (log_allocation("mmap", (size_t)n, arr, arr,
                       vmsize_before, vmrss_before, vmsize_after, vmrss_after) != 0) {
        fprintf(stderr, "Peringatan: pencatatan log mmap tidak lengkap.\n");
    }

    // lepas pemetaan memorinya pake munmap
    munmap(arr, bytes);
}

void fitur_variasi() {
    // TODO: paging & lazy allocation
}

// Loop Menu Utama Program
int main() {
    int pilihan;

    while (1) {
        // Panggil fungsi banner & menu dari module terpisah
        cetak_banner();
        cetak_menu();

        // Get input dari user
        if (scanf("%d", &pilihan) != 1) {
            printf("\nInput tidak valid. Masukkan angka.\n\n");
            // Bersihin buffer input biar terminal tidak nge-spam infinite loop
            while (getchar() != '\n');
            continue;
        }

        // Logika menu utama
        if (pilihan == 1) {
            alokasi_malloc();
        } else if (pilihan == 2) {
            alokasi_mmap();
        } else if (pilihan == 3) {
            fitur_variasi();
        } else if (pilihan == 4) {
            printf("\nKeluar dari program...\n");
            break; // Keluar dari while loop
        } else {
            printf("\nInput tidak valid. Silakan mencoba lagi.\n\n");
        }
    }

    return 0;
}
