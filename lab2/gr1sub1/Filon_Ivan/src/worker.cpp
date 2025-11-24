#include <iostream>
#include <unistd.h>
#include <csignal>
#include <atomic>
#include <chrono>
#include <sched.h>
#include <sys/resource.h>

using namespace std;

class Worker {
public:
    struct ModeConfig { int work_us; int sleep_us; };
    enum class WorkMode { LIGHT = 1, HEAVY = 2 };

private:
    static inline atomic<bool> running{true};
    static inline atomic<WorkMode> currentMode{WorkMode::HEAVY};

    ModeConfig heavyConfig;
    ModeConfig lightConfig;
    int tickCounter;

    static void handleTerm(int) { running = false; }
    static void handleLight(int) { currentMode = WorkMode::LIGHT; }
    static void handleHeavy(int) { currentMode = WorkMode::HEAVY; }

    void doWork(const ModeConfig &cfg) {
        usleep(cfg.work_us);
        usleep(cfg.sleep_us);
    }

    void showStatus() {
        ++tickCounter;
        int cpu = sched_getcpu();
        int niceVal = getpriority(PRIO_PROCESS, 0);

        cout << "[WORKER] PID=" << getpid()
             << " | Mode=" << (currentMode == WorkMode::HEAVY ? "HEAVY" : "LIGHT")
             << " | Tick=" << tickCounter
             << " | CPU=" << cpu
             << " | Nice=" << niceVal
             << endl;
    }

public:
    Worker(int hw, int hs, int lw, int ls, WorkMode initial)
        : heavyConfig{hw, hs}, lightConfig{lw, ls}, tickCounter(0) 
    {
        currentMode = initial;
    }

    void setupSignals() {
        signal(SIGTERM, handleTerm);
        signal(SIGUSR1, handleLight);
        signal(SIGUSR2, handleHeavy);
    }

    void run() {
        setupSignals();
        while (running) {
            ModeConfig cfg = (currentMode == WorkMode::HEAVY ? heavyConfig : lightConfig);
            doWork(cfg);
            showStatus();
        }
        cout << "[WORKER] PID=" << getpid() << " exiting gracefully\n";
    }
};

int main(int argc, char* argv[]) {
    if (argc < 6) {
        cerr << "Usage: " << argv[0] << " <heavy_work_us> <heavy_sleep_us> <light_work_us> <light_sleep_us> <mode>\n";
        return 1;
    }

    int hw = stoi(argv[1]);
    int hs = stoi(argv[2]);
    int lw = stoi(argv[3]);
    int ls = stoi(argv[4]);
    auto initialMode = static_cast<Worker::WorkMode>(stoi(argv[5]));

    Worker w(hw, hs, lw, ls, initialMode);
    w.run();
    return 0;
}
