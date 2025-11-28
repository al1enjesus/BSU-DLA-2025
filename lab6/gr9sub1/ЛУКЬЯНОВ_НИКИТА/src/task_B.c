#include "myfuse.h"

// ROT13 алгоритм (Симметричный: ROT13(ROT13(x)) == x)
// Применяется только к буквам латинского алфавита
static void apply_rot13(char *buf, size_t size) {
    for (size_t i = 0; i < size; i++) {
        char c = buf[i];
        if ((c >= 'a' && c <= 'm') || (c >= 'A' && c <= 'M')) {
            buf[i] += 13;
        } else if ((c >= 'n' && c <= 'z') || (c >= 'N' && c <= 'Z')) {
            buf[i] -= 13;
        }
    }
}

static int do_read_with_rot13(const char *path, char *buf, size_t size, off_t offset,
                   struct fuse_file_info *fi) {
    int res = pread(fi->fh, buf, size, offset);
    
    if (res == -1) {
        res = -errno;
    } else {
        apply_rot13(buf, res); 
    }

    log_msg("READ", path, res);
    return res;
}

static int do_write_with_rot13(const char *path, const char *buf, size_t size,
                    off_t offset, struct fuse_file_info *fi) {
    char *enc_buf = malloc(size);
    if (!enc_buf) return -ENOMEM;
    
    memcpy(enc_buf, buf, size);
    
    apply_rot13(enc_buf, size);

    int res = pwrite(fi->fh, enc_buf, size, offset);
    
    free(enc_buf);

    if (res == -1)
        res = -errno;

    log_msg("WRITE", path, res);
    return res;
}

int main(int argc, char* argv[]) {
    operations.read = do_read_with_rot13;
    operations.write = do_write_with_rot13;

    return myfuse_main(argc, argv);
}