#include <iostream>
#include <csignal>
#include <atomic>
#include <unistd.h>
#include <sched.h>
#include <stdexcept>

using namespace std;

class Worker {
public:
    struct Mode { int work_us; int sleep_us; };
    enum class WorkerMode { LIGHT, HEAVY };

private:
    static inline atomic<bool> running{true};
    static inline atomic<WorkerMode> mode{WorkerMode::HEAVY};
    Mode heavy, light;
    int tick = 0;

    static void sigterm(int){ running=false; }
    static void sigusr1(int){ mode = WorkerMode::LIGHT; }
    static void sigusr2(int){ mode = WorkerMode::HEAVY; }

    void doWork(const Mode& m) {
        usleep(m.work_us);
        usleep(m.sleep_us);
    }

    void logStatus() {
        tick++;
        int cpu = sched_getcpu();
        cout << "[WORKER " << getpid() << "] Mode="
             << (mode==WorkerMode::HEAVY?"HEAVY":"LIGHT")
             << " Tick=" << tick
             << " CPU=" << cpu << "\n";
    }

public:
    Worker(int hw, int hs, int lw, int ls, WorkerMode m) : heavy{hw,hs}, light{lw,ls} { mode=m; }

    void setupSignals() {
        signal(SIGTERM,sigterm);
        signal(SIGUSR1,sigusr1);
        signal(SIGUSR2,sigusr2);
    }

    void run() {
        setupSignals();
        while(running) {
            Mode m = (mode==WorkerMode::HEAVY?heavy:light);
            doWork(m);
            logStatus();
        }
        cout << "[WORKER " << getpid() << "] exiting\n";
    }
};

int safeStoi(const char* arg) {
    try {
        return stoi(arg);
    } catch (const exception& e) {
        cerr << "Invalid argument: " << arg << " (" << e.what() << ")\n";
        exit(1);
    }
}

int main(int argc,char* argv[]) {
    if(argc<6){
        cerr<<"Usage: "<<argv[0]<<" <heavy_work> <heavy_sleep> <light_work> <light_sleep> <mode>\n";
        return 1;
    }
    int hw = safeStoi(argv[1]);
    int hs = safeStoi(argv[2]);
    int lw = safeStoi(argv[3]);
    int ls = safeStoi(argv[4]);
    int modeInt = safeStoi(argv[5]);

    auto m = static_cast<Worker::WorkerMode>(modeInt);
    Worker w(hw,hs,lw,ls,m);
    w.run();
    return 0;
}

