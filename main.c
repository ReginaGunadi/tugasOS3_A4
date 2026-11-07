#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/mman.h> // NOTE: ini kalo merah, berarti karena lagi di OS Windows
                    // nanti kalo di-run di terminal Linux, akan aman. (Wait lagi ngecek)
#include "banner.h" // Panggil modul banner

#define GROUP_PREFIX "A4"
#define LOG_BUFFER_SIZE 1024

// write() bisa menulis sebagian data atau terhenti oleh sinyal (EINTR).
// Ulangi sampai semua byte berhasil ditulis.
static int write_all(int fd, const char *buffer, size_t length) {
    size_t written = 0;
    while (written < length) {
        ssize_t result = write(fd, buffer + written, length - written);
        if (result < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        written += (size_t)result;
    }
    return 0;
}

// Tambahkan teks berformat; cek hasil vsnprintf agar isi tidak melewati buffer.
// Return -1 jika ruang buffer tidak cukup.
static int append(char *buffer, size_t size, size_t *used, const char *format, ...) {
    va_list args;
    va_start(args, format);
    int length = vsnprintf(buffer + *used, size - *used, format, args);
    va_end(args);

    if (length < 0 || (size_t)length >= size - *used) return -1;
    *used += (size_t)length;
    return 0;
}

// Catat satu alokasi ke dua berkas. Return 0 jika keduanya berhasil, -1 jika gagal.
static int log_allocation(const char *method, size_t allocation_count,
                          const int *values, const void *address,
                          long vmsize_before, long vmrss_before,
                          long vmsize_after, long vmrss_after) {
    char timestamp[32];
    char std_path[128];
    char sys_path[128];
    struct timespec now;
    struct tm local;

    if (!method || (strcmp(method, "malloc") != 0 && strcmp(method, "mmap") != 0) ||
        (allocation_count > 0 && !values)) {
        errno = EINVAL;
        perror("logger: argumen tidak valid");
        return -1;
    }

    // Ambil waktu sekali agar pasangan log punya timestamp sama.
    if (clock_gettime(CLOCK_REALTIME, &now) != 0 ||
        localtime_r(&now.tv_sec, &local) == NULL ||
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d_%H-%M-%S", &local) == 0) {
        perror("logger: gagal membuat timestamp");
        return -1;
    }
    if (snprintf(std_path, sizeof(std_path), GROUP_PREFIX "-%s_std-%s.txt",
                 method, timestamp) >= (int)sizeof(std_path) ||
        snprintf(sys_path, sizeof(sys_path), GROUP_PREFIX "-%s_sys-%s.txt",
                 method, timestamp) >= (int)sizeof(sys_path)) {
        errno = ENAMETOOLONG;
        perror("logger: nama berkas terlalu panjang");
        return -1;
    }

    // Susun isi sekali agar kedua berkas log identik.
    char content[LOG_BUFFER_SIZE];
    size_t used = 0;
    size_t shown = allocation_count < 10 ? allocation_count : 10;
    int build_failed = 0;

    if (append(content, sizeof(content), &used,
               "You picked %s/%s.\n"
               "VmSize Before: %ld KB | VmRSS Before: %ld KB\n"
               "Starting memory address: %p\n"
               "Integers allocated: %zu\n"
               "Printing first 10 values in reverse order:\n",
               method, strcmp(method, "malloc") == 0 ? "free" : "munmap",
               vmsize_before, vmrss_before, address, allocation_count) != 0) {
        build_failed = 1;
    }

    for (size_t i = 0; i < shown && !build_failed; ++i) {
        if (append(content, sizeof(content), &used, "%d\n",
                   values[allocation_count - 1 - i]) != 0) {
            build_failed = 1;
        }
    }
    if (!build_failed &&
        append(content, sizeof(content), &used,
               "VmSize After: %ld KB | VmRSS After: %ld KB\n",
               vmsize_after, vmrss_after) != 0) {
        build_failed = 1;
    }
    if (build_failed) {
        fprintf(stderr, "logger: isi log terlalu besar untuk buffer\n");
        return -1;
    }

    // Jalur stdio memakai fopen/fprintf/fclose dan buffer di ruang user.
    // Statusnya terpisah agar kegagalan jalur ini tidak menghentikan jalur sys.
    int std_failed = 0;
    FILE *file = fopen(std_path, "w");
    if (!file) {
        std_failed = 1;
    } else {
        if (fprintf(file, "%s", content) < 0) std_failed = 1;
        if (fclose(file) != 0) std_failed = 1;
    }
    if (std_failed) perror(std_path);

    // Jalur system call memakai open/write/close tanpa buffer stdio.
    int sys_failed = 0;
    // 0644: pemilik bisa baca/tulis, pengguna lain hanya bisa membaca.
    int fd = open(sys_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        sys_failed = 1;
    } else {
        if (write_all(fd, content, used) != 0) sys_failed = 1;
        if (close(fd) != 0) sys_failed = 1;
    }
    if (sys_failed) perror(sys_path);

    return (std_failed || sys_failed) ? -1 : 0;
}

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
        printf("Input tidak valid!\n\n");
        while (getchar() != '\n');
        return;
    }

    // minta memori ke heap pake malloc
    int *arr = (int *)malloc(n * sizeof(int));
    if (arr == NULL) {
        perror("Proses malloc gagal!");
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
        printf("Input tidak valid!\n\n");
        while (getchar() != '\n');
        return;
    }

    size_t bytes = n * sizeof(int);

    // minta memori langsung ke kernel pake mmap anonymous
    int *arr = (int *)mmap(NULL, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (arr == MAP_FAILED) {
        perror("Proses mmap gagal!");
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
    printf("\n--- Varian 4: Demand Paging ---\n");

    long long total_mb_can_use = 1000;
    long long total_bytes_can_use = total_mb_can_use * 1024 * 1024;

    printf("Alokasi virtual: %lld MB\n\n", total_mb_can_use);

    // Coba buat dulu alokasi memorinya pakai mmap
    int *arr_variation = (int *)mmap(NULL, total_bytes_can_use, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (arr_variation == MAP_FAILED) {
        perror("Proses mmap gagal!");
        return;
    }

    // Berapa kali kondisi memori ditampilkan (sesuai contoh output di soal)
    int checkpoints[] = {0, 5, 10, 25, 50, 75, 100}; 
    int checkpoints_size = sizeof(checkpoints) / sizeof(checkpoints[0]);

    printf("%-10s %-10s %-15s %-15s %-15s\n", "Disentuh", "Halaman", "VmSize (KB)", "VmRSS (KB)", "delta RSS (KB)");

    // Penghitungan nilai dari halaman, VMSize, VmRSS, dan delta RSS per checkpoint
    long long vmrss_prev = -1;
    long long bytes_touched_prev = 0;

    for (int i = 0; i < checkpoints_size; i++) {
        int touch_mega_bytes = checkpoints[i];
        long long touch_bytes = touch_mega_bytes * 1024 * 1024;

        // Halaman = bytes / 4 KB --> 4 KB = 4096 bytes  
        long long page_num = touch_bytes / 4096;
        
        // Urus memori per halaman (4 KB / 4096 bytes) dari batas disentuh sebelumnya sampai ke checkpoint
        // Kalau ada butuh akses/nyentuh memori --> lakukan page fault agar OS mengalokasikan memori (demand paging)
        if (touch_mega_bytes > 0){
            long long int_size = (long long)sizeof(int);
            for (long long b = bytes_touched_prev / int_size; b < touch_bytes / int_size; b += 4096 / int_size) {
                arr_variation[b] = 1; // Nulis ke memori ngebuat page fault
            }
            bytes_touched_prev = touch_bytes;
        }

        // Ambil nilai VMSize dan VMRSS
        long long vmsize_now = get_vmsize_kb();
        long long vmrss_now = get_vmrss_kb();
        
        // Penghitungan delta RSS
        // Rumus: Delta_rss = vmrss_now - vmrss_prev
        long long delta_rss;

        if (i == 0){
            delta_rss = 0;
            vmrss_prev = vmrss_now;
        } 
        else{
            delta_rss = vmrss_now - vmrss_prev;
            vmrss_prev = vmrss_now;
        }

        printf("%-10d MB %-10lld %-15lld %-15lld %+lld\n", touch_mega_bytes, page_num, vmsize_now, vmrss_now, delta_rss);
    }

    munmap(arr_variation, total_bytes_can_use);
    printf("\n");
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
