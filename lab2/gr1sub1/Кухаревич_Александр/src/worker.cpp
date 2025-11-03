#include <bits/stdc++.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>
#include <sched.h>
#include <sys/resource.h>

using namespace std;

struct ModeConfig { long work_us = 9000; long sleep_us = 1000; };
struct Config { ModeConfig heavy; ModeConfig light; };

volatile sig_atomic_t worker_terminate = 0;
volatile sig_atomic_t worker_mode_switch = 0;
void sigterm_handler(int){ worker_terminate = 1; }
void sigusr1_handler(int){ worker_mode_switch = 1; }
void sigusr2_handler(int){ worker_mode_switch = 2; }

bool load_config(const string &path, Config &cfg) {
    ifstream f(path);
    if(!f) return false;
    string line;
    auto trim=[](string s){size_t a=s.find_first_not_of(" \t\r\n"); if(a==string::npos) return string(); size_t b=s.find_last_not_of(" \t\r\n"); return s.substr(a,b-a+1);};
    while(getline(f,line)){
        auto p=line.find('#'); if(p!=string::npos) line=line.substr(0,p);
        line=trim(line); if(line.empty()) continue;
        auto eq=line.find('='); if(eq==string::npos) continue;
        string k=trim(line.substr(0,eq)); string v=trim(line.substr(eq+1));
        try {
            if(k=="mode_heavy_work_us") cfg.heavy.work_us=stol(v);
            else if(k=="mode_heavy_sleep_us") cfg.heavy.sleep_us=stol(v);
            else if(k=="mode_light_work_us") cfg.light.work_us=stol(v);
            else if(k=="mode_light_sleep_us") cfg.light.sleep_us=stol(v);
        } catch(...) {}
    }
    return true;
}

int main(int argc, char **argv){
    if(argc < 3){
        cerr << "Usage: worker <slot_id> <config_path> [groupA|groupB]\n";
        return 1;
    }

    int slot = stoi(argv[1]);
    string cfg_path = argv[2];
    string group = (argc>=4)?argv[3]:"";

    Config cfg;
    load_config(cfg_path, cfg);
    ModeConfig mode = cfg.heavy;
    int current_mode = 2;

    struct sigaction sa_term{};
    sa_term.sa_handler = sigterm_handler; sigemptyset(&sa_term.sa_mask); sa_term.sa_flags=0; sigaction(SIGTERM,&sa_term,nullptr);
    struct sigaction sa_u1{};
    sa_u1.sa_handler = sigusr1_handler; sigemptyset(&sa_u1.sa_mask); sa_u1.sa_flags=0; sigaction(SIGUSR1,&sa_u1,nullptr);
    struct sigaction sa_u2{};
    sa_u2.sa_handler = sigusr2_handler; sigemptyset(&sa_u2.sa_mask); sa_u2.sa_flags=0; sigaction(SIGUSR2,&sa_u2,nullptr);

    setvbuf(stdout, nullptr, _IONBF, 0);

    pid_t pid = getpid();
    unsigned long ticks = 0;
    int nicev = getpriority(PRIO_PROCESS, 0);
#ifdef __linux__
    int cpu = -1;
    cpu = sched_getcpu();
    cpu_set_t st; CPU_ZERO(&st);
    sched_getaffinity(0, sizeof(st), &st);
    string mask="";
    int ncpus = sysconf(_SC_NPROCESSORS_ONLN);
    for(int i=0;i<ncpus;i++) if(CPU_ISSET(i,&st)) mask += to_string(i)+",";
    if(!mask.empty()) mask.pop_back();
#else
    int cpu = -1;
    string mask="?";
#endif
    printf("[worker] PID=%d slot=%d group=%s nice=%d cpu_now=%d affinity=%s start\n", pid, slot, group.c_str(), nicev, cpu, mask.c_str());

    while(!worker_terminate){
        if(worker_mode_switch==1){ current_mode=1; mode=cfg.light; worker_mode_switch=0; }
        if(worker_mode_switch==2){ current_mode=2; mode=cfg.heavy; worker_mode_switch=0; }

        long work_us = mode.work_us;
        struct timeval start, now;
        gettimeofday(&start,nullptr);
        while(true){
            if(worker_terminate) break;
            gettimeofday(&now,nullptr);
            long elapsed=(now.tv_sec-start.tv_sec)*1000000L+(now.tv_usec-start.tv_usec);
            if(elapsed>=work_us) break;
            double x=0; for(int i=0;i<200;i++) x+=sqrt(i*3.1415);
        }

        ticks++;
        if(ticks%5==0){
#ifdef __linux__
            int cpu_now = sched_getcpu();
#else
            int cpu_now = -1;
#endif
            int cur_nice = getpriority(PRIO_PROCESS, 0);
            printf("[worker] PID=%d slot=%d mode=%s ticks=%lu cpu=%d nice=%d\n",
                   pid,slot,(current_mode==2?"HEAVY":"LIGHT"),ticks,cpu_now,cur_nice);
        }

        long sleep_us = mode.sleep_us, slept=0;
        const long chunk=20000;
        while(slept<sleep_us && !worker_terminate){
            long to=min(chunk,sleep_us-slept);
            usleep(to);
            slept+=to;
        }
    }

    printf("[worker] PID=%d exiting. ticks=%lu\n", pid, ticks);
    return 0;
}
