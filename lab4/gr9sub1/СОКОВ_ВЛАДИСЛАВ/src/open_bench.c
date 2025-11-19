#include <fcntl.h>
#include <stdio.h>
#include <x86intrin.h>

int main() {
    const char *f="/tmp/lab4_bench_testfile";

    for (int i=0;i<100000;i++) {
        unsigned aux;
        unsigned long s=__rdtscp(&aux);
        int fd=open(f,0);

        if(fd>=0) close(fd);

        unsigned long e=__rdtscp(&aux);
        
        if(i<10) printf("open iteration %d cycles=%lu\n", i, e-s);
    }

    return 0;
}
