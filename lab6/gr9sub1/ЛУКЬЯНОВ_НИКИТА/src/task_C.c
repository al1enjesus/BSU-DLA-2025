#include "myfuse.h"

//Перевод букв в верхний регистр
static void apply_uppercase(char *buf, size_t size) {
    for (size_t i = 0; i < size; i++) {
        buf[i] = toupper(buf[i]);
    }
}

static int do_read_uppercase(const char *path, char *buf, size_t size, off_t offset,
                   struct fuse_file_info *fi) {
    int res = pread(fi->fh, buf, size, offset);
    
    if (res == -1) {
        res = -errno;
    } else {
        apply_uppercase(buf, res); 
    }

    log_msg("READ", path, res);
    return res;
}

int main(int argc, char* argv[]) {
    operations.read = do_read_uppercase;

    return myfuse_main(argc, argv);
}