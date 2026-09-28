#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>

long raw_write(int fd, const char *buf, size_t count) {
    long result;

    // Explicitly bind C variables to x86_64 Syscall ABI registers
    register long rax __asm__("rax") = __NR_write; // Syscall #1 (write)
    register long rdi __asm__("rdi") = fd;          // Arg 1: file descriptor
    register const char *rsi __asm__("rsi") = buf; // Arg 2: buffer pointer
    register size_t rdx __asm__("rdx") = count;    // Arg 3: byte count

    __asm__ volatile (
        "syscall"
        : "=a" (result)
        : "r" (rax), "r" (rdi), "r" (rsi), "r" (rdx)
        : "rcx", "r11", "memory"
    );

    return result;
}

int main(void) {
    const char *message = "Direct kernel system call execution!\n";
    raw_write(STDOUT_FILENO, message, 37);
    return 0;
}
