#include <stdio.h>
#include <stdlib.h>

void trigger_crash(int *target_ptr) {
    printf("[Function] Attempting to write to pointer address: %p\n", (void *)target_ptr);
    *target_ptr = 0xDEADBEEF; // Triggers SIGSEGV
}

int main(void) {
    printf("[Main] Initializing diagnostic crash program...\n");
    int *unmapped_memory = NULL;
    trigger_crash(unmapped_memory);
    return 0;
}
