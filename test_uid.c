// Based on https://unix.stackexchange.com/questions/166817/using-the-setuid-bit-properly

#define _POSIX_C_SOURCE 200112L // Needed with glibc (e.g., linux).
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

void report () {
    printf ("Real UID: %d Effective UID: %d\n",
        getuid(),
        geteuid()
    );
}

int main (void) {
    uid_t real;
    report();
    real = getuid();
    seteuid(real);
    report();
    return 0;
}
