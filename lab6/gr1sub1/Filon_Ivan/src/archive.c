#define FUSE_USE_VERSION 31

#include <fuse3/fuse.h>
#include <archive.h>
#include <archive_entry.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <limits.h>

#define MAX_FILES 1024

typedef struct {
    char fpath[PATH_MAX];
    mode_t fmode;
    size_t fsize;
    off_t data_offset;
    time_t mtime;
} arc_file_t;

static arc_file_t arc_files[MAX_FILES];
static char *arc_name = NULL;
static int arc_count = 0;

static arc_file_t* arc_find(const char *path) {
    for (int i = 0; i < arc_count; i++) {
        if (!strcmp(arc_files[i].fpath, path))
            return &arc_files[i];
    }
    return NULL;
}

static int arc_load() {
    struct archive *arc = archive_read_new();
    struct archive_entry *ent;

    archive_read_support_filter_all(arc);
    archive_read_support_format_all(arc);

    if (archive_read_open_filename(arc, arc_name, 10240) != ARCHIVE_OK) {
        fprintf(stderr, "Ошибка: %s\n", archive_error_string(arc));
        archive_read_free(arc);
        return -1;
    }

    arc_count = 0;

    while (archive_read_next_header(arc, &ent) == ARCHIVE_OK) {

        const char *p = archive_entry_pathname(ent);

        if (!p || !strcmp(p, ".") || !strcmp(p, "./")) {
            archive_read_data_skip(arc);
            continue;
        }

        const char *clean = p;
        if (!strncmp(p, "./", 2)) clean = p + 2;
        if (!clean[0]) {
            archive_read_data_skip(arc);
            continue;
        }

        arc_file_t *af = &arc_files[arc_count];

        if (clean[0] != '/')
            snprintf(af->fpath, PATH_MAX, "/%s", clean);
        else
            strncpy(af->fpath, clean, PATH_MAX);

        size_t len = strlen(af->fpath);
        if (len > 1 && af->fpath[len - 1] == '/')
            af->fpath[len - 1] = '\0';

        af->fmode = archive_entry_mode(ent);
        af->fsize = archive_entry_size(ent);
        af->mtime = archive_entry_mtime(ent);
        af->data_offset = archive_read_header_position(arc);

        fprintf(stderr, "Загружено: %s (%zu байт)\n", af->fsize, af->fpath);

        arc_count++;
        if (arc_count >= MAX_FILES)
            break;

        archive_read_data_skip(arc);
    }

    archive_read_free(arc);
    fprintf(stderr, "Файлов загружено: %d\n", arc_count);
    return 0;
}

static int arc_getattr(const char *path, struct stat *st, struct fuse_file_info *fi) {
    (void) fi;
    memset(st, 0, sizeof(*st));

    if (!strcmp(path, "/")) {
        st->st_mode = S_IFDIR | 0755;
        st->st_nlink = 2;
        return 0;
    }

    arc_file_t *f = arc_find(path);
    if (!f) {

        size_t plen = strlen(path);
        for (int i = 0; i < arc_count; i++) {
            if (!strncmp(arc_files[i].fpath, path, plen) &&
                arc_files[i].fpath[plen] == '/' &&
                strlen(arc_files[i].fpath) > plen) {

                st->st_mode = S_IFDIR | 0755;
                st->st_nlink = 2;
                return 0;
            }
        }

        return -ENOENT;
    }

    st->st_mode = f->fmode;
    st->st_nlink = 1;
    st->st_size = f->fsize;
    st->st_mtime = f->mtime;

    return 0;
}

static int arc_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                       off_t off, struct fuse_file_info *fi, enum fuse_readdir_flags fl) {

    (void) off; (void) fi; (void) fl;

    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);

    size_t plen = strlen(path);
    int root = !strcmp(path, "/");

    char added[MAX_FILES][128];
    int added_n = 0;

    for (int i = 0; i < arc_count; i++) {
        const char *fp = arc_files[i].fpath;
        const char *nm = NULL;

        if (root) {
            if (fp[0] == '/' && fp[1])
                nm = fp + 1;
        } else {
            if (!strncmp(fp, path, plen)) {
                if (fp[plen] == '/' && fp[plen + 1])
                    nm = fp + plen + 1;
                else if (fp[plen] == '\0')
                    continue;
            }
        }

        if (!nm) continue;

        char out[128];
        const char *slash = strchr(nm, '/');
        int isdir = 0;

        if (slash) {
            size_t n = slash - nm;
            if (n >= sizeof(out)) n = sizeof(out) - 1;
            strncpy(out, nm, n);
            out[n] = 0;
            isdir = 1;
        } else {
            strncpy(out, nm, sizeof(out));
            out[sizeof(out)-1] = 0;
            isdir = S_ISDIR(arc_files[i].fmode);
        }

        int seen = 0;
        for (int k = 0; k < added_n; k++)
            if (!strcmp(added[k], out))
                seen = 1;

        if (!seen && added_n < MAX_FILES) {
            struct stat st;
            memset(&st, 0, sizeof(st));
            st.st_mode = isdir ? (S_IFDIR | 0755) : (S_IFREG | 0644);

            filler(buf, out, &st, 0, 0);
            strcpy(added[added_n++], out);
        }
    }
    return 0;
}

static int arc_open(const char *p, struct fuse_file_info *fi) {
    arc_file_t *f = arc_find(p);
    if (!f) return -ENOENT;
    if ((fi->flags & O_ACCMODE) != O_RDONLY) return -EACCES;
    return 0;
}

static int arc_read(const char *path, char *buf, size_t sz, off_t off,
                    struct fuse_file_info *fi) {

    (void) fi;

    arc_file_t *f = arc_find(path);
    if (!f) return -ENOENT;

    if (off >= (off_t) f->fsize) return 0;
    if (off + sz > f->fsize) sz = f->fsize - off;

    struct archive *a = archive_read_new();
    struct archive_entry *e;

    archive_read_support_filter_all(a);
    archive_read_support_format_all(a);

    if (archive_read_open_filename(a, arc_name, 10240) != ARCHIVE_OK) {
        archive_read_free(a);
        return -EIO;
    }

    while (archive_read_next_header(a, &e) == ARCHIVE_OK) {

        const char *pn = archive_entry_pathname(e);
        if (!strcmp(pn, ".") || !strcmp(pn, "./")) {
            archive_read_data_skip(a);
            continue;
        }

        const char *cp = pn;
        if (!strncmp(pn, "./", 2)) cp = pn + 2;
        if (!cp[0]) {
            archive_read_data_skip(a);
            continue;
        }

        char np[PATH_MAX];
        if (cp[0] != '/')
            snprintf(np, PATH_MAX, "/%s", cp);
        else
            strncpy(np, cp, PATH_MAX);

        size_t L = strlen(np);
        if (L > 1 && np[L - 1] == '/') np[L - 1] = 0;

        if (!strcmp(np, path)) {

            if (off > 0) {
                char tmp[4096];
                size_t r = off;
                while (r) {
                    size_t t = (r > sizeof(tmp)) ? sizeof(tmp) : r;
                    ssize_t rr = archive_read_data(a, tmp, t);
                    if (rr <= 0) break;
                    r -= rr;
                }
            }

            ssize_t rd = archive_read_data(a, buf, sz);
            archive_read_free(a);
            return rd > 0 ? rd : 0;
        }

        archive_read_data_skip(a);
    }

    archive_read_free(a);
    return -ENOENT;
}

static struct fuse_operations arc_ops = {
    .getattr = arc_getattr,
    .readdir = arc_readdir,
    .open    = arc_open,
    .read    = arc_read
};

int main(int argc, char *argv[]) {

    if (argc < 3) {
        fprintf(stderr, "Использование: %s <архив.tar> <точка_монтирования>\n", argv[0]);
        return 1;
    }

    arc_name = realpath(argv[1], NULL);
    if (!arc_name) {
        perror("realpath");
        return 1;
    }

    fprintf(stderr, "Загрузка архива: %s\n", arc_name);
    if (arc_load() != 0) {
        free(arc_name);
        return 1;
    }

    for (int i = 1; i < argc - 1; i++)
        argv[i] = argv[i + 1];
    argc--;

    fprintf(stderr, "Монтирование → %s\n", argv[1]);
    int r = fuse_main(argc, argv, &arc_ops, NULL);

    free(arc_name);
    return r;
}
