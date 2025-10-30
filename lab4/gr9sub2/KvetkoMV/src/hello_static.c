#include <stdio.h>
int main() {
    FILE *f = fopen("/tmp/test.txt", "w");
    if (f) {
        fprintf(f, "Hello from static binary\n");
        fclose(f);
    }
    printf("Done\n");
    return 0;
}
