#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cstring>
#include <cstdlib>
#include <pthread.h>

using namespace std;
using namespace chrono;

static const int ITER = 20000; // разумный объём (можно увеличить)
static const char* EXE = "/bin/true";

inline uint64_t ns_since(const high_resolution_clock::time_point& start) {
    return (uint64_t)duration_cast<nanoseconds>(high_resolution_clock::now() - start).count();
}

int dummy() { return 42; }

void bench_dummy() {
    auto start = high_resolution_clock::now();
    for (int i=0;i<ITER;i++) dummy();
    uint64_t ns = ns_since(start);
    cout << "[dummy]        iters="<<ITER<<", total="<<ns<<" ns, avg="<<(double)ns/ITER<<" ns\n";
}

void bench_getpid() {
    auto start = high_resolution_clock::now();
    for (int i=0;i<ITER;i++) (void)getpid();
    uint64_t ns = ns_since(start);
    cout << "[getpid]       iters="<<ITER<<", total="<<ns<<" ns, avg="<<(double)ns/ITER<<" ns\n";
}

void bench_fork_wait() {
    auto start = high_resolution_clock::now();
    for (int i=0;i<ITER;i++) {
        pid_t p = fork();
        if (p == 0) {
            // child
            _exit(0);
        } else if (p > 0) {
            int st=0;
            waitpid(p, &st, 0);
        } else {
            // fork error
            perror("fork");
            break;
        }
    }
    uint64_t ns = ns_since(start);
    cout << "[fork+wait]    iters="<<ITER<<", total="<<ns<<" ns, avg="<<(double)ns/ITER<<" ns\n";
}

void bench_fork_execve() {
    auto start = high_resolution_clock::now();
    for (int i=0;i<ITER;i++) {
        pid_t p = fork();
        if (p == 0) {
            // child does execve /bin/true
            char *const argv[] = { (char*)"true", NULL };
            char *const envp[] = { NULL };
            execve(EXE, argv, envp);
            // if execve failed:
            _exit(127);
        } else if (p > 0) {
            int st=0;
            waitpid(p, &st, 0);
        } else {
            perror("fork");
            break;
        }
    }
    uint64_t ns = ns_since(start);
    cout << "[fork+execve]  iters="<<ITER<<", total="<<ns<<" ns, avg="<<(double)ns/ITER<<" ns\n";
}

struct ThreadArg { int id; };
void* thread_fn(void* a) {
    (void)a;
    return (void*)0;
}

void bench_pthread_create() {
    auto start = high_resolution_clock::now();
    for (int i=0;i<ITER;i++) {
        pthread_t t;
        if (pthread_create(&t, nullptr, thread_fn, nullptr) == 0) {
            pthread_join(t, nullptr);
        } else {
            perror("pthread_create");
            break;
        }
    }
    uint64_t ns = ns_since(start);
    cout << "[pthread_create] iters="<<ITER<<", total="<<ns<<" ns, avg="<<(double)ns/ITER<<" ns\n";
}

int main() {
    cout << "=== Syscall/process benchmark (variant 10) ===\n";
    bench_dummy();
    bench_getpid();
    bench_pthread_create();
    bench_fork_wait();
    bench_fork_execve();
    cout << "=== End ===\n";
    return 0;
}

