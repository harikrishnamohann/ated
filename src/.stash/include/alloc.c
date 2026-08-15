#include <sys/mman.h>
#include <unistd.h>
#include "utils.h"

int main() {
    void* ptr = mmap(NULL, GB(4), PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    mprotect(ptr, KB(4), PROT_READ | PROT_WRITE);
    madvise((char*)ptr + (10 * 1024 * 1024), 10 * 1024 * 1024, MADV_DONTNEED);
    
}


