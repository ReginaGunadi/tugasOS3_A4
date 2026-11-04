#define _POSIX_C_SOURCE 200809L
#include "logger.h"

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

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
// method: "malloc" atau "mmap"; allocation_count: jumlah integer yang dialokasikan.
// values: array alokasi; address: alamat awal memori.
// Metrik VmSize dan VmRSS sebelum/sesudah dinyatakan dalam KB.
int log_allocation(const char *method, size_t allocation_count,
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
