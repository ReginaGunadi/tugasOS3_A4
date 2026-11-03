#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "banner.h" // Panggil modul banner

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
    // TODO: alokasi malloc/free
}

void alokasi_mmap() {
    // TODO: alokasi mmap/munmap
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