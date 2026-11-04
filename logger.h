#ifndef LOGGER_H
#define LOGGER_H

#include <stddef.h>

// Catat alokasi ke dua log. method: "malloc" atau "mmap"; return 0 jika sukses.
int log_allocation(const char *method, size_t allocation_count,
                   const int *values, const void *address,
                   long vmsize_before, long vmrss_before,
                   long vmsize_after, long vmrss_after);

#endif
