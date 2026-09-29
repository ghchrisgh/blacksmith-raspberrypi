// pagemap.cpp – virt to phys. pagemap

#include "pagemap.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

uint64_t get_phys_base(void* virt_base)
{
    uint64_t entry;
    long pagesize = sysconf(_SC_PAGESIZE);

    // Byte-Offset in /proc/self/pagemap: (virt / pagesize) × 8 Byte
    uint64_t offset = ((uint64_t)virt_base / (uint64_t)pagesize) * sizeof(entry);

    int fd = open("/proc/self/pagemap", O_RDONLY);
    if (fd < 0) {
        perror("[ ERR ] - open(/proc/self/pagemap) – run as root");
        exit(1);
    }

    int bytes = pread(fd, &entry, sizeof(entry), (off_t)offset);
    close(fd);

    if (bytes != 8) {
        fprintf(stderr, "[ ERR ] - pagemap pread: %d bytes (expected 8)\n", bytes);
        exit(1);
    }

    // Bit 63: page present
    if (!(entry & (1ULL << 63))) {
        fprintf(stderr, "[ ERR ] - pagemap: page not present – fault-in missing?\n");
        exit(1);
    }

    // Bits 0-54: PFN (page frame number)
    uint64_t pfn = entry & 0x3fffffffffffffULL;
    if (pfn == 0) {
        fprintf(stderr, "[ ERR ] - pagemap: PFN=0 – root required\n");
        exit(1);
    }

    return pfn * (uint64_t)pagesize;
}
