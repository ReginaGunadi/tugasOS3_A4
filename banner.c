#include <stdio.h>
#include "banner.h"

void cetak_banner() {
    printf("=== KELOMPOK A4 - TUGAS MEMORI ===\n");
}

// Fungsi buat menampilkan menu
void cetak_menu() {
    printf("1. Allocate with malloc/free\n");
    printf("2. Allocate with mmap/munmap\n");
    printf("3. Demand Paging & Alokasi Lambat (Fitur Variasi)\n");
    printf("4. Exit\n");
    printf("Input: ");
}