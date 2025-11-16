#include <iostream>
#include <fstream>
#include <vector>
#include <deque>
#include <algorithm>
#include <csignal>
#include <unistd.h>
#include <sys/wait.h>
#include <chrono>
#include <mutex>
#include <atomic>
#include <sys/resource.h>
#include <sched.h>
#include <yaml-cpp/yaml.h>

using namespace std;

class Supervisor {
public:
    struct Mode { int work_us; int sleep_us; };
    struct Config {
        int workers;
        Mode heavy;
        Mode light;
    };
    enum class WorkerMode { LIGHT, HEAVY };

private:
    static inline Config cfg{};
    static inline vector<pid_t> workerPIDs{};
    static inline string configPath{};
    static inline string workerPath{};
    static inline atomic<bool> terminateFlag{false};
    static inline atomic<bool> reloadFlag{false};
    static inline atomic<WorkerMode> currentMode{WorkerMode::HEAVY};
    static inline mutex workerMutex{};
    static inline deque<chrono::steady_clock::time_point> restartTimes{};
    static inline mutex restartMutex{};

    static constexpr int SHUTDOWN_TIMEOUT = 5;
    static constexpr int RESTART_WINDOW = 30;
    static constexpr int MAX_RESTARTS = 5;

    Supervisor() = default;

    static void readConfig() {
        try {
            auto yaml = YAML::LoadFile(configPath);
            cfg.workers = yaml["workers"].as<int>();
            cfg.heavy.work_us = yaml["heavy"]["work_us"].as<int>();
            cfg.heavy.sleep_us = yaml["heavy"]["sleep_us"].as<int>();
            cfg.light.work_us = yaml["light"]["work_us"].as<int>();
            cfg.light.sleep_us = yaml["light"]["sleep_us"].as<int>();
        } catch (...) {
            cerr << "[SUP] Failed to read config, using defaults\n";
            cfg.workers = 3;
            cfg.heavy = {4000000, 1000000};
            cfg.light = {500000, 1500000};
        }
    }

    static void spawnWorker() {
        pid_t pid = fork();
        if (pid < 0) {
            cerr << "[SUP] Fork failed\n";
            return;
        }
        if (pid == 0) {
            // ==== NICE ====
            int nice_val = workerPIDs.size() % 2 == 0 ? 10 : 0;
            if (setpriority(PRIO_PROCESS, 0, nice_val) != 0)
                perror("setpriority");

            // ==== CPU-AFFINITY ====
            cpu_set_t mask;
            CPU_ZERO(&mask);
            if (workerPIDs.size() % 2 == 0)
                CPU_SET(0, &mask);
            else
                CPU_SET(1, &mask);
            if (sched_setaffinity(0, sizeof(mask), &mask) != 0)
                perror("sched_setaffinity");

            execl(workerPath.c_str(), workerPath.c_str(),
                  to_string(cfg.heavy.work_us).c_str(),
                  to_string(cfg.heavy.sleep_us).c_str(),
                  to_string(cfg.light.work_us).c_str(),
                  to_string(cfg.light.sleep_us).c_str(),
                  to_string(static_cast<int>(currentMode.load())).c_str(),
                  nullptr);
            _exit(1);
        } else {
            lock_guard<mutex> lock(workerMutex);
            workerPIDs.push_back(pid);
            cout << "[SUP] Worker started PID=" << pid << " nice=" 
                 << (workerPIDs.size()%2==0?10:0)
                 << " CPU=" << (workerPIDs.size()%2==0?0:1) << "\n";
        }
    }

    static void stopWorkers() {
        lock_guard<mutex> lock(workerMutex);
        for (auto pid : workerPIDs) kill(pid, SIGTERM);

        auto deadline = chrono::steady_clock::now() + chrono::seconds(SHUTDOWN_TIMEOUT);
        while (!workerPIDs.empty() && chrono::steady_clock::now() < deadline) {
            int status;
            pid_t pid = waitpid(-1, &status, WNOHANG);
            if (pid > 0) workerPIDs.erase(remove(workerPIDs.begin(), workerPIDs.end(), pid), workerPIDs.end());
            else usleep(100000);
        }

        for (auto pid : workerPIDs) kill(pid, SIGKILL);
        while (waitpid(-1, nullptr, 0) > 0);
        workerPIDs.clear();
    }

    static void handleSigchld(int) {
        int status;
        pid_t pid;
        while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
            cout << "[SUP] Worker exited PID=" << pid << "\n";
            lock_guard<mutex> lock(workerMutex);
            workerPIDs.erase(remove(workerPIDs.begin(), workerPIDs.end(), pid), workerPIDs.end());

            if (!terminateFlag && !reloadFlag) {
                auto now = chrono::steady_clock::now();
                lock_guard<mutex> rlock(restartMutex);
                restartTimes.push_back(now);
                while (!restartTimes.empty() &&
                       chrono::duration_cast<chrono::seconds>(now - restartTimes.front()).count() > RESTART_WINDOW) {
                    restartTimes.pop_front();
                }
                if ((int)restartTimes.size() <= MAX_RESTARTS) spawnWorker();
                else cerr << "[SUP] Too many restarts, skipping\n";
            }
        }
    }

    static void sigTermHandler(int) { terminateFlag = true; }
    static void sigReloadHandler(int) { reloadFlag = true; }
    static void sigLightHandler(int) { currentMode = WorkerMode::LIGHT; sendSignalToWorkers(SIGUSR1); }
    static void sigHeavyHandler(int) { currentMode = WorkerMode::HEAVY; sendSignalToWorkers(SIGUSR2); }

    static void sendSignalToWorkers(int sig) {
        lock_guard<mutex> lock(workerMutex);
        for (auto pid : workerPIDs) kill(pid, sig);
    }

    static void setupSignals() {
        signal(SIGCHLD, handleSigchld);
        signal(SIGTERM, sigTermHandler);
        signal(SIGINT, sigTermHandler);
        signal(SIGHUP, sigReloadHandler);
        signal(SIGUSR1, sigLightHandler);
        signal(SIGUSR2, sigHeavyHandler);
    }

public:
    static void init(const string& cfgPath, const string& wPath) {
        configPath = cfgPath;
        workerPath = wPath;
        setupSignals();
        readConfig();
    }

    static void run() {
        for (int i = 0; i < cfg.workers; i++) spawnWorker();

        while (true) {
            if (terminateFlag) {
                cout << "[SUP] Shutting down...\n";
                stopWorkers();
                break;
            }
            if (reloadFlag) {
                cout << "[SUP] Reloading config...\n";
                readConfig();
                stopWorkers();
                for (int i = 0; i < cfg.workers; i++) spawnWorker();
                reloadFlag = false;
            }
            pause();
        }
    }
};

int main(int argc, char *argv[]) {
    if (argc < 3) {
        cerr << "Usage: " << argv[0] << " <config.yaml> <worker_binary>\n";
        return 1;
    }
    Supervisor::init(argv[1], argv[2]);
    Supervisor::run();
    return 0;
}
