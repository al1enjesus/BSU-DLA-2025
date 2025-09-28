#include <iostream>
#include <fstream>
#include <sstream>
#include <csignal>
#include <unistd.h>
#include <sys/wait.h>
#include <vector>
#include <map>
#include <deque>
#include <mutex>
#include <atomic>
#include <chrono>
#include <algorithm>
#include <yaml-cpp/yaml.h>
#include <sched.h>
#include <sys/resource.h>
#include <cstring>

using namespace std;

class Supervisor {
public:
    struct ModeConfig { int work_us; int sleep_us; };
    struct Config {
        int workerCount;
        ModeConfig heavy;
        ModeConfig light;
        int schedulingPolicy;
    };
    enum class WorkMode { LIGHT=1, HEAVY=2 };
    enum class Policy { DEFAULT, MIXED_NICE, CPU_AFFINITY };

private:
    static constexpr int SHUTDOWN_TIMEOUT=5;
    static constexpr int RESTART_WINDOW=30;
    static constexpr int MAX_RESTARTS=5;
    static constexpr int STATUS_CHECK_US=200000;

    static inline Config cfg{};
    static inline vector<pid_t> pids{};
    static inline map<pid_t,int> pidIndices{};
    static inline deque<chrono::steady_clock::time_point> restartTimes{};
    static inline atomic<bool> terminateFlag{false};
    static inline atomic<bool> reloadFlag{false};
    static inline atomic<WorkMode> mode{WorkMode::HEAVY};
    static inline Policy policy{Policy::DEFAULT};
    static inline string configPath{};
    static inline string workerBinary{};
    static inline mutex mtxPids{};
    static inline mutex mtxIndices{};
    static inline int availableCPUs{1};
    static inline int nextIndex{0};

    Supervisor() = default;

    static void handleChild(int) {
        int status;
        pid_t pid;
        while ((pid=waitpid(-1,&status,WNOHANG))>0) {
            int idx = -1;
            {
                lock_guard<mutex> lock(mtxIndices);
                auto it = pidIndices.find(pid);
                if (it!=pidIndices.end()) { idx=it->second; pidIndices.erase(it); }
            }
            cout << "[SUP] Worker " << pid << " (idx=" << idx << ") finished\n";
            {
                lock_guard<mutex> lock(mtxPids);
                pids.erase(remove(pids.begin(),pids.end(),pid),pids.end());
            }

            if (!terminateFlag) {
                auto now=chrono::steady_clock::now();
                lock_guard<mutex> lock(mtxIndices);
                restartTimes.push_back(now);
                while(!restartTimes.empty() && chrono::duration_cast<chrono::seconds>(now-restartTimes.front()).count()>RESTART_WINDOW)
                    restartTimes.pop_front();
                if ((int)restartTimes.size()<=MAX_RESTARTS) spawnWorker(idx!=-1?idx:nextIndex++);
                else cerr<<"[SUP] Too many restarts, skipping...\n";
            }
        }
    }

    static void handleTerminate(int) { terminateFlag=true; }
    static void handleReload(int) { reloadFlag=true; }
    static void switchLight(int) { 
        mode=WorkMode::LIGHT; 
        cout << "[SUP] Switching to LIGHT mode\n"; 
        lock_guard<mutex> lock(mtxPids);
        for (auto pid:pids) kill(pid,SIGUSR1);
    }
    static void switchHeavy(int) { 
        mode=WorkMode::HEAVY; 
        cout << "[SUP] Switching to HEAVY mode\n"; 
        lock_guard<mutex> lock(mtxPids);
        for (auto pid:pids) kill(pid,SIGUSR2);
    }

    static void readConfig() {
        try {
            YAML::Node y=YAML::LoadFile(configPath);
            cfg.workerCount=y["workers"].as<int>();
            cfg.heavy={y["heavy"]["work_us"].as<int>(),y["heavy"]["sleep_us"].as<int>()};
            cfg.light={y["light"]["work_us"].as<int>(),y["light"]["sleep_us"].as<int>()};
            cfg.schedulingPolicy=y["scheduling_policy"].as<int>(0);
            setPolicy(cfg.schedulingPolicy);
        } catch(...) {
            cerr<<"[SUP] Failed to read YAML, using defaults\n";
            cfg.workerCount=2; cfg.heavy={5000000,1000000}; cfg.light={1000000,500000}; cfg.schedulingPolicy=0;
            setPolicy(cfg.schedulingPolicy);
        }
    }

    static void setupSystem() {
        cpu_set_t cpuset; CPU_ZERO(&cpuset);
        if(sched_getaffinity(0,sizeof(cpuset),&cpuset)==0) availableCPUs=CPU_COUNT(&cpuset);
        else { cerr<<"[SUP] Cannot get CPU count\n"; availableCPUs=1; }
        cout << "[SUP] Available CPUs: " << availableCPUs << "\n";
    }

    static void setPolicy(int p) {
        switch(p){
            case 1: policy=Policy::MIXED_NICE; cout<<"[SUP] Policy: MIXED NICE\n"; break;
            case 2: policy=Policy::CPU_AFFINITY; cout<<"[SUP] Policy: CPU AFFINITY\n"; break;
            default: policy=Policy::DEFAULT; cout<<"[SUP] Policy: DEFAULT\n"; break;
        }
    }

    static void applyPolicy(int idx){
        switch(policy){
            case Policy::MIXED_NICE: setpriority(PRIO_PROCESS,0,(idx%2==0?10:0)); break;
            case Policy::CPU_AFFINITY: if(availableCPUs>1){ cpu_set_t c; CPU_ZERO(&c); CPU_SET(idx%availableCPUs,&c); sched_setaffinity(0,sizeof(c),&c); } break;
            default: break;
        }
    }

    static void spawnWorker(int idx){
        pid_t pid=fork();
        if(pid<0){ cerr<<"[SUP] Fork failed\n"; return; }
        if(pid==0){
            applyPolicy(idx);
            execl(workerBinary.c_str(),workerBinary.c_str(),
                to_string(cfg.heavy.work_us).c_str(),
                to_string(cfg.heavy.sleep_us).c_str(),
                to_string(cfg.light.work_us).c_str(),
                to_string(cfg.light.sleep_us).c_str(),
                to_string((int)mode.load()).c_str(),
                nullptr);
            _exit(1);
        }
        {
            lock_guard<mutex> lock(mtxPids);
            pids.push_back(pid);
        }
        lock_guard<mutex> lock(mtxIndices);
        pidIndices[pid]=idx;
        cout<<"[SUP] Spawned worker PID="<<pid<<" idx="<<idx<<"\n";
    }

    static void stopWorkers() {
        lock_guard<mutex> lock(mtxPids);
        for(auto pid:pids) kill(pid,SIGTERM);
        auto deadline=chrono::steady_clock::now()+chrono::seconds(SHUTDOWN_TIMEOUT);
        while(!pids.empty() && chrono::steady_clock::now()<deadline){
            int status; pid_t pid=waitpid(-1,&status,WNOHANG);
            if(pid>0) pids.erase(remove(pids.begin(),pids.end(),pid),pids.end());
            else usleep(STATUS_CHECK_US);
        }
        for(auto pid:pids) { kill(pid,SIGKILL); waitpid(pid,nullptr,0); }
        pids.clear();
    }

    static void setupSignals() {
        signal(SIGCHLD,handleChild);
        signal(SIGTERM,handleTerminate);
        signal(SIGINT,handleTerminate);
        signal(SIGHUP,handleReload);
        signal(SIGUSR1,switchLight);
        signal(SIGUSR2,switchHeavy);
    }

public:
    static void init(const string &confFile,const string &workerBin){
        configPath=confFile; workerBinary=workerBin;
        setupSignals(); setupSystem(); readConfig();
    }

    static void run() {
        nextIndex=cfg.workerCount;
        for(int i=0;i<cfg.workerCount;i++) spawnWorker(i);
        while(true){
            if(terminateFlag){ cout<<"[SUP] Terminating supervisor...\n"; stopWorkers(); break; }
            if(reloadFlag){ cout<<"[SUP] Reloading config...\n"; readConfig(); stopWorkers(); for(int i=0;i<cfg.workerCount;i++) spawnWorker(i); reloadFlag=false; }
            pause();
        }
    }
};

int main(int argc,char* argv[]){
    if(argc<3){ cerr<<"Usage: "<<argv[0]<<" <config_file> <worker_path>\n"; return 1; }
    Supervisor::init(argv[1],argv[2]);
    Supervisor::run();
    return 0;
}
