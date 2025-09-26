#ifndef SUPERVISOR_H
#define SUPERVISOR_H

#include <vector>
#include <unistd.h>
#include <iostream>
#include <signal.h>
#include <wait.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <memory>
#include <chrono> 
#include <fstream>

#include "ConfigSharedMemory.h"

struct WorkerInfo {
    pid_t pid;
    std::chrono::steady_clock::time_point lastRestart;
    int restartCount;
};


class Supervisor {
public:
    static bool init() {
        sInstance = new Supervisor();
        signal(SIGTERM, handleSignal);
        signal(SIGINT, handleSignal);
        signal(SIGHUP, handleSignal);
        signal(SIGUSR1, handleSignal);
        signal(SIGUSR2, handleSignal);

        if(!sInstance->initSharedMemory())
            return false;

        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
        pthread_mutex_init(&sInstance->mWorkerConfig->mtx, &attr);
        
        auto configResult = readConfigFile();

        pthread_mutex_lock(&sInstance->mWorkerConfig->mtx);
        
        sInstance->mWorkerConfig->heavyWorkUs = configResult.heavyWorkUs;
        sInstance->mWorkerConfig->heavySleepUs = configResult.heavySleepUs;
        sInstance->mWorkerConfig->lightWorkUs = configResult.lightWorkUs;
        sInstance->mWorkerConfig->lightSleepUs = configResult.lightSleepUs;

        pthread_mutex_unlock(&sInstance->mWorkerConfig->mtx);

        if(!sInstance->createWorkers(configResult.workers)) {
            gracefullyEnd();
            return false;
        }

        return true;
    }

    static Supervisor * get() {
        return sInstance;
    }

    static int exec() {
        std::cout << "[Supervisor " << getpid() << "] Start execution...\n";
        while(!sInstance->shutdownFlag) {
                if(sInstance->reloadFlag) {
                std::cout << "[Supervisor " << getpid() << "] Reloading config...\n";
                
                auto configResult = readConfigFile();
                sInstance->shareConfigResult(configResult);

                for(const auto& worker : sInstance->mWorkers) {
                    kill(worker.pid, SIGHUP);
                }

                sInstance->reloadFlag = 0;
            }

            if(sInstance->lightModeFlag) {
                for(const auto& worker : sInstance->mWorkers) {
                    kill(worker.pid, SIGUSR1);
                }
                
                std::cout << "[Supervisor " << getpid() << "] Light mode Enabled\n";
                sInstance->lightModeFlag = 0;
            }

            if(sInstance->heavyModeFlag) {
                for(const auto& worker : sInstance->mWorkers) {
                    kill(worker.pid, SIGUSR2);
                }
                
                std::cout << "[Supervisor " << getpid() << "] Heavy mode enabled\n";
                sInstance->heavyModeFlag = 0;
            }

            //std::cout << "[Supervisor" << getpid() << "] Start waiting...\n";
            int status;
            pid_t pid = waitpid(-1, &status, WNOHANG);
            //std::cout << "[Supervisor" << getpid() << "] end waiting.\n";
            
            if(pid > 0) {
                sInstance->handleExitedWorker(pid);
            }

            usleep(200000);
        }

        gracefullyEnd();

        return 0;
    }

private:
    Supervisor() = default;

    static void handleSignal(int sig) {
        if(sig == SIGTERM || sig == SIGINT) sInstance->shutdownFlag = 1;
        else if(sig == SIGHUP) sInstance->reloadFlag = 1;
        else if(sig == SIGUSR1) sInstance->lightModeFlag = 1;
        else if(sig == SIGUSR2) sInstance->heavyModeFlag = 1;
        else if(sig == SIGCHLD) sInstance->showChildrenInfo = 1;
    }

    bool initSharedMemory() {
        int fd = shm_open("/shared_worker_config", O_CREAT | O_RDWR, 0666);
        
        if(fd == -1) {
            std::cout << "Failed to create shared memory!\n";
            return false;
        }

        ftruncate(fd, sizeof(WorkerConfig));
        
        mWorkerConfig = reinterpret_cast<WorkerConfig*>(
            mmap(nullptr, sizeof(WorkerConfig), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)
        );
        
        if(!mWorkerConfig) {
            std::cout << "Failed to map shared memory\n";
            return false;
        }
        
        return true;
    }

    bool createWorkers(const int N) {
        if(!mWorkerConfig) {
            std::cout << "[Supervisor " << getpid() << "] Shared memory wasn't initialized!\n";
            return false;
        }   

        mWorkers.resize(N);
        std::cout << "[Supervisor " << getpid() << "] Creating workers...\n";
        for(int i = 0; i < N; ++i) {
            if(!createWorker(i))
                return false;
        }
        std::cout << "[Supervisor " << getpid() << "] Done.\n";
        return true;
    }

    bool createWorker(const int i, const int restartCount = 0) {
        pid_t pid = fork();
   
        if(pid == -1) {
            std::cout << "[Supervisor " << getpid() << "] Some error occluded\n";
            return false;
        }
        else if (pid == 0) {
            execl("./worker", "./worker", nullptr);
            perror("execl failed");
            exit(1);
        }
        else {
            mWorkers[i] = {pid, std::chrono::steady_clock::now(), restartCount};
            std::cout << "[Supervisor " << getpid() << "] New Worker " <<  mWorkers[i].pid << "\n";
        }

        return true;
    }

    void handleExitedWorker(const pid_t pid) {
        std::cout << "[Supervisor " << getpid() << "] Worker " << pid << " exited, restarting...\n";

        int exitedWokerIndex = 0;
        for(;exitedWokerIndex < mWorkers.size(); ++exitedWokerIndex) {
            if(mWorkers[exitedWokerIndex].pid == pid)
                break;
        }

        auto now = std::chrono::steady_clock::now();
        
        if(std::chrono::duration_cast<std::chrono::seconds>(now - mWorkers[exitedWokerIndex].lastRestart).count() > 30)
            mWorkers[exitedWokerIndex].restartCount = 0;

        if(mWorkers[exitedWokerIndex].restartCount < 5) {
            createWorker(exitedWokerIndex, ++mWorkers[exitedWokerIndex].restartCount);
        } 
        else {
            std::cout << "[Supervisor " << getpid() << "] Too many restarts for worker " << pid << "\n";
        }
    }

    static void gracefullyEnd() {
        std::cout << "[Supervisor " << getpid() << "] Shutting down workers...\n";
        for(auto &w : sInstance->mWorkers) {
            std::cout << "[Supervisor " << getpid() << "] Try to gracefully end worker " << w.pid << "\n"; 
            kill(w.pid, SIGTERM);
        }
        
        for(int i=0; i < sInstance->mWorkers.size(); i++) 
            wait(nullptr);

        pthread_mutex_destroy(&sInstance->mWorkerConfig->mtx);
        shm_unlink("/shared_worker_config");

        std::cout << "[Supervisor " << getpid() << "] Exiting\n";
    }

    void shareConfigResult(const ConfigData& configResult) {
        pthread_mutex_lock(&sInstance->mWorkerConfig->mtx);
        
        sInstance->mWorkerConfig->heavyWorkUs = configResult.heavyWorkUs;
        sInstance->mWorkerConfig->heavySleepUs = configResult.heavySleepUs;
        sInstance->mWorkerConfig->lightWorkUs = configResult.lightWorkUs;
        sInstance->mWorkerConfig->lightSleepUs = configResult.lightSleepUs;

        pthread_mutex_unlock(&sInstance->mWorkerConfig->mtx);
    }
    
    // struct CpuUsage {
    //     double userTimeSec;
    //     double systemTimeSec;
    // };

    // CpuUsage getCpuUsage(pid_t pid) {
    //     std::string path = "/proc/" + std::to_string(pid) + "/stat";
    //     std::ifstream statFile(path);
    //     if (!statFile.is_open()) return {0, 0};

    //     std::string tmp;
    //     long utime = 0, stime = 0;
    //     for (int i = 1; i <= 15; i++) {
    //         statFile >> tmp;
    //         if (i == 14) utime = std::stol(tmp);
    //         if (i == 15) stime = std::stol(tmp);
    //     }

    //     long ticksPerSec = sysconf(_SC_CLK_TCK);
    //     return { (double)utime / ticksPerSec, (double)stime / ticksPerSec };
    // }

    // int getLastCpu(pid_t pid) {
    //     std::string path = "/proc/" + std::to_string(pid) + "/stat";
    //     std::ifstream statFile(path);
    //     if (!statFile.is_open()) return -1;

    //     std::string tmp;
    //     for (int i = 1; i <= 39; i++) statFile >> tmp;
    //     return std::stoi(tmp);
    // }

    // void printWorkersStatus() {
    //     std::cout << "[Supervisor " << getpid() << "] Workers status:\n";
    //     for (const auto &w : mWorkers) {
    //         CpuUsage usage = getCpuUsage(w.pid);
    //         int cpu = getLastCpu(w.pid);

    //         std::cout << " PID=" << w.pid
    //                 << " User=" << usage.userTimeSec << "s"
    //                 << " Sys=" << usage.systemTimeSec << "s"
    //                 << " LastCPU=" << cpu
    //                 << " Restarts=" << (int)w.restartCount
    //                 << "\n";
    //     }
    // }

private:
    static Supervisor* sInstance;
    WorkerConfig* mWorkerConfig = nullptr;
    std::vector<WorkerInfo> mWorkers;

    volatile sig_atomic_t shutdownFlag = 0;
    volatile sig_atomic_t reloadFlag = 0;
    volatile sig_atomic_t lightModeFlag = 0;
    volatile sig_atomic_t heavyModeFlag = 0;
    volatile sig_atomic_t showChildrenInfo = 0;
};

Supervisor* Supervisor::sInstance = nullptr;

#endif //SUPEVISOR_H