
#define FUSE_USE_VERSION 31

#include <assert.h>     #include <ctype.h>      #include <dirent.h>
#include <errno.h>      #include <fcntl.h>      #include <fuse.h>
#include <limits.h>     #include <stddef.h>     #include <stdio.h>
#include <stdlib.h>     #include <string.h>     #include <sys/stat.h>
#include <sys/types.h>  #include <sys/xattr.h>  #include <time.h>
#include <unistd.h>

static char* ROOT_DIRECTORY = NULL;


static void LOG_OP(const char* OP, const char* PATH, int RES) {
    time_t T = time(NULL);
    char TS[64];
    strftime(TS, sizeof(TS), "%Y-%m-%d %H:%M:%S", localtime(&T));
    fprintf(stderr, "[%s] %s: %s (result: %d)\n", TS, OP, PATH, RES);
}



static void BUILD_PATH(char* DEST, const char* SRC) {
    char TMP[PATH_MAX];
    if (strcmp(SRC, "/") == 0) { strcpy(TMP, ROOT_DIRECTORY); }
    else { snprintf(TMP, PATH_MAX, "%s%s", ROOT_DIRECTORY, SRC); }
    if (realpath(TMP, DEST) == NULL) {
        strncpy(DEST, TMP, PATH_MAX);
        DEST[PATH_MAX - 1] = '\0';
        return;
    }
    size_t LEN = strlen(ROOT_DIRECTORY);
    if (strncmp(DEST, ROOT_DIRECTORY, LEN) != 0) {
        fprintf(stderr, "SECURITY: Blocked path traversal: %s\n", SRC);
        strcpy(DEST, ROOT_DIRECTORY);
    }
}


static void TO_UPPER(char* DATA, size_t N) {
    for (size_t i = 0; i < N; i++) {
        DATA[i] = toupper((unsigned char)DATA[i]);
    }
}



static int FS_GETATTR(const char* P, struct stat* S, struct fuse_file_info* FI) {
    (void)FI; char FP[PATH_MAX]; BUILD_PATH(FP, P);
    int R = lstat(FP, S);
    if (R == -1) { LOG_OP("GETATTR", P, -errno); return -errno; }
    LOG_OP("GETATTR", P, 0); return 0;
}

static int FS_READDIR(const char* P, void* B, fuse_fill_dir_t F,
                     off_t O, struct fuse_file_info* FI,
                     enum fuse_readdir_flags FL) {
    (void)O;(void)FI;(void)FL; char FP[PATH_MAX]; BUILD_PATH(FP, P);
    DIR* D = opendir(FP); if (!D) { LOG_OP("READDIR", P, -errno); return -errno; }
    struct dirent* DE; int R = 0;
    while ((DE = readdir(D))) {
        struct stat ST = {0}; ST.st_ino = DE->d_ino; ST.st_mode = DE->d_type << 12;
        if (F(B, DE->d_name, &ST, 0, 0)) { R = -ENOMEM; break; }
    }
    closedir(D); LOG_OP("READDIR", P, R); return R;
}

static int FS_OPEN(const char* P, struct fuse_file_info* FI) {
    char FP[PATH_MAX]; BUILD_PATH(FP, P);
    int FD = open(FP, FI->flags);
    if (FD == -1) { LOG_OP("OPEN", P, -errno); return -errno; }
    close(FD); LOG_OP("OPEN", P, 0); return 0;
}

static int FS_READ(const char* P, char* B, size_t SZ, off_t O,
                  struct fuse_file_info* FI) {
    (void)FI; char FP[PATH_MAX]; BUILD_PATH(FP, P);
    int FD = open(FP, O_RDONLY);
    if (FD == -1) { LOG_OP("READ", P, -errno); return -errno; }
    int R = pread(FD, B, SZ, O);
    if (R > 0) { TO_UPPER(B, R); }
    if (R == -1) { R = -errno; }
    close(FD); LOG_OP("READ", P, R); return R;
}

static int FS_WRITE(const char* P, const char* B, size_t SZ,
                   off_t O, struct fuse_file_info* FI) {
    (void)FI; char FP[PATH_MAX]; BUILD_PATH(FP, P);
    int FD = open(FP, O_WRONLY);
    if (FD == -1) { LOG_OP("WRITE", P, -errno); return -errno; }
    int R = pwrite(FD, B, SZ, O);
    if (R == -1) { R = -errno; }
    close(FD); LOG_OP("WRITE", P, R); return R;
}

static int FS_CREATE(const char* P, mode_t M, struct fuse_file_info* FI) {
    (void)FI; char FP[PATH_MAX]; BUILD_PATH(FP, P);
    int FD = creat(FP, M);
    if (FD == -1) { LOG_OP("CREATE", P, -errno); return -errno; }
    close(FD); LOG_OP("CREATE", P, 0); return 0;
}

static int FS_UNLINK(const char* P) {
    char FP[PATH_MAX]; BUILD_PATH(FP, P);
    int R = unlink(FP);
    if (R == -1) { LOG_OP("UNLINK", P, -errno); return -errno; }
    LOG_OP("UNLINK", P, 0); return 0;
}

static int FS_MKDIR(const char* P, mode_t M) {
    char FP[PATH_MAX]; BUILD_PATH(FP, P);
    int R = mkdir(FP, M);
    if (R == -1) { LOG_OP("MKDIR", P, -errno); return -errno; }
    LOG_OP("MKDIR", P, 0); return 0;
}

static int FS_RMDIR(const char* P) {
    char FP[PATH_MAX]; BUILD_PATH(FP, P);
    int R = rmdir(FP);
    if (R == -1) { LOG_OP("RMDIR", P, -errno); return -errno; }
    LOG_OP("RMDIR", P, 0); return 0;
}

static int FS_TRUNCATE(const char* P, off_t SZ, struct fuse_file_info* FI) {
    (void)FI; char FP[PATH_MAX]; BUILD_PATH(FP, P);
    int R = truncate(FP, SZ);
    if (R == -1) { LOG_OP("TRUNCATE", P, -errno); return -errno; }
    LOG_OP("TRUNCATE", P, 0); return 0;
}

static int FS_CHMOD(const char* P, mode_t M, struct fuse_file_info* FI) {
    (void)FI; char FP[PATH_MAX]; BUILD_PATH(FP, P);
    int R = chmod(FP, M);
    if (R == -1) { LOG_OP("CHMOD", P, -errno); return -errno; }
    LOG_OP("CHMOD", P, 0); return 0;
}

static int FS_CHOWN(const char* P, uid_t U, gid_t G, struct fuse_file_info* FI) {
    (void)FI; char FP[PATH_MAX]; BUILD_PATH(FP, P);
    int R = lchown(FP, U, G);
    if (R == -1) { LOG_OP("CHOWN", P, -errno); return -errno; }
    LOG_OP("CHOWN", P, 0); return 0;
}



static struct fuse_operations FUSE_OPS = {
    .getattr  = FS_GETATTR,  .readdir  = FS_READDIR,  .open   = FS_OPEN,
    .read     = FS_READ,     .write    = FS_WRITE,    .create = FS_CREATE,
    .unlink   = FS_UNLINK,   .mkdir    = FS_MKDIR,    .rmdir  = FS_RMDIR,
    .truncate = FS_TRUNCATE, .chmod    = FS_CHMOD,    .chown  = FS_CHOWN,
};



int main(int A, char** V) {
    if (A < 3) {
        fprintf(stderr, "ИСПОЛЬЗОВАНИЕ: %s <каталог> <точка_монтирования> [опции]\n", V[0]);
        return 1;
    }
    ROOT_DIRECTORY = realpath(V[1], NULL);
    if (!ROOT_DIRECTORY) { perror("realpath"); return 1; }
    fprintf(stderr, "МОНТИРОВАНИЕ: %s → %s\n", ROOT_DIRECTORY, V[2]);
    
    int FUSE_A = A - 1;
    char** FUSE_V = malloc(sizeof(char*) * FUSE_A);
    if (!FUSE_V) { perror("malloc"); free(ROOT_DIRECTORY); return 1; }
    
    FUSE_V[0] = V[0];
    for (int I = 2; I < A; I++) { FUSE_V[I-1] = V[I]; }
    
    int R = fuse_main(FUSE_A, FUSE_V, &FUSE_OPS, NULL);
    
    free(FUSE_V); free(ROOT_DIRECTORY);
    return R;
}
