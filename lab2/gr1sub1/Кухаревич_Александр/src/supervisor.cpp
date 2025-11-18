#include <bits/stdc++.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/resource.h>
#include <sched.h>

using namespace std;

struct ModeConfig { long work_us; long sleep_us; };
struct Config {
    int workers = 3;
    ModeConfig heavy{9000,1000};
    ModeConfig light{2000,8000};
    int restart_limit_count = 5;
    int restart_limit_window_s = 30;
    int config_reload_wait_ms = 200;
};

Config g_config;
string g_config_path = "config.cfg";

bool load_config(const string &path, Config &out){
    ifstream f(path);
    if(!f) return false;
    Config c = out;
    string line;
    auto trim=[](string s){size_t a=s.find_first_not_of(" \t\r\n"); if(a==string::npos) return string(); size_t b=s.find_last_not_of(" \t\r\n"); return s.substr(a,b-a+1);};
    while(getline(f,line)){
        auto p=line.find('#'); if(p!=string::npos) line=line.substr(0,p);
        line=trim(line); if(line.empty()) continue;
        auto eq=line.find('='); if(eq==string::npos) continue;
        string k=trim(line.substr(0,eq)); string v=trim(line.substr(eq+1));
        try{
            if(k=="workers") c.workers=stoi(v);
            else if(k=="restart_limit_count") c.restart_limit_count=stoi(v);
            else if(k=="restart_limit_window_s") c.restart_limit_window_s=stoi(v);
            else if(k=="config_reload_wait_ms") c.config_reload_wait_ms=stoi(v);
        }catch(...){ }
    }
    out=c; return true;
}

int sig_pipe[2];
void write_sig(char c){ write(sig_pipe[1], &c, 1); }
void h_chld(int){ write_sig('C'); }
void h_term(int){ write_sig('T'); }
void h_hup (int){ write_sig('H'); }
void h_u1  (int){ write_sig('1'); }
void h_u2  (int){ write_sig('2'); }

struct WorkerSlot {
    pid_t pid=-1;
    int slot_id=-1;
    deque<time_t> restarts;
};

int get_nprocs(){
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    return (n>0)?(int)n:1;
}

void spawn_worker(WorkerSlot &s, int slot_id){
    pid_t pid=fork();
    if(pid==0){
        int ncpus = get_nprocs();
        int cpu0 = 0;
        int cpu1 = (ncpus>1)?1:0;

        if(slot_id % 2 == 0){
            errno = 0;
            if(setpriority(PRIO_PROCESS, 0, 0) != 0 && errno!=0){}
            cpu_set_t cpus; CPU_ZERO(&cpus); CPU_SET(cpu1, &cpus);
            sched_setaffinity(0, sizeof(cpus), &cpus);
            execl("./worker", "worker", to_string(slot_id).c_str(), g_config_path.c_str(), "groupA", nullptr);
        } else {
            if(setpriority(PRIO_PROCESS, 0, 10) != 0){ /* ignore */ }
            cpu_set_t cpus; CPU_ZERO(&cpus); CPU_SET(cpu0, &cpus);
            sched_setaffinity(0, sizeof(cpus), &cpus);
            execl("./worker", "worker", to_string(slot_id).c_str(), g_config_path.c_str(), "groupB", nullptr);
        }
        _exit(2);
    }
    s.pid = pid;
    s.slot_id = slot_id;
    s.restarts.push_back(time(nullptr));
    cout << "[supervisor] spawned slot="<<slot_id<<" pid="<<pid<<"\n";
}

int main(int argc, char **argv){
    for(int i=1;i<argc;i++){
        string a=argv[i];
        if((a=="-c"||a=="--config") && i+1<argc) g_config_path=argv[++i];
    }
    load_config(g_config_path, g_config);

    pipe(sig_pipe);
    fcntl(sig_pipe[0],F_SETFL,O_NONBLOCK);
    fcntl(sig_pipe[1],F_SETFL,O_NONBLOCK);

    signal(SIGCHLD,h_chld);
    signal(SIGTERM,h_term);
    signal(SIGINT ,h_term);
    signal(SIGHUP ,h_hup);
    signal(SIGUSR1,h_u1);
    signal(SIGUSR2,h_u2);

    setvbuf(stdout,nullptr,_IONBF,0);

    int N = max(2, g_config.workers);
    vector<WorkerSlot> slots(N);
    for(int i=0;i<N;i++) spawn_worker(slots[i], i);

    bool terminating=false;
    while(true){
        char c;
        if(read(sig_pipe[0], &c, 1)<=0){ usleep(50000); continue; }

        if(c=='C'){
            while(true){
                int status; pid_t pid=waitpid(-1,&status,WNOHANG);
                if(pid<=0) break;
                int sid=-1; for(int j=0;j<N;j++) if(slots[j].pid==pid) sid=j;
                if(sid<0) continue;
                bool abnormal = !(WIFEXITED(status) && WEXITSTATUS(status)==0);
                slots[sid].pid=-1;

                time_t now=time(nullptr);
                while(!slots[sid].restarts.empty() && now - slots[sid].restarts.front() > g_config.restart_limit_window_s)
                    slots[sid].restarts.pop_front();

                if(!terminating && (int)slots[sid].restarts.size() < g_config.restart_limit_count){
                    if(abnormal) {
                        cerr << "[supervisor] worker slot="<<sid<<" pid="<<pid<<" died unexpectedly, restarting...\n";
                    } else {
                        cerr << "[supervisor] worker slot="<<sid<<" pid="<<pid<<" exited, respawning...\n";
                    }
                    spawn_worker(slots[sid], sid);
                } else if(!terminating){
                    cerr << "[supervisor] restart limit reached for slot="<<sid<<"; not restarting now.\n";
                }
            }
        }
        else if(c=='T'){
            terminating=true;
            cout << "[supervisor] sending SIGTERM to workers...\n";
            for(auto&s:slots) if(s.pid>0) kill(s.pid,SIGTERM);
            for(int t=0;t<50;t++){
                bool all=false;
                all=true;
                for(auto&s:slots) if(s.pid>0) { all=false; break; }
                if(all) break;
                usleep(100000);
            }
            for(auto&s:slots) if(s.pid>0) kill(s.pid,SIGKILL);
            while(waitpid(-1,nullptr,WNOHANG)>0);
            cout << "[supervisor] exit\n";
            return 0;
        }
        else if(c=='H'){
            cout << "[supervisor] reload config\n";
            load_config(g_config_path, g_config);
            int newN=max(2,g_config.workers);
            vector<WorkerSlot> ns(newN);
            for(int j=0;j<newN;j++) spawn_worker(ns[j], j);
            usleep(g_config.config_reload_wait_ms*1000);
            for(auto&s:slots) if(s.pid>0) kill(s.pid,SIGTERM);
            while(waitpid(-1,nullptr,WNOHANG)>0);
            slots.swap(ns); N=newN;
        }
        else if(c=='1' || c=='2'){
            int sig=(c=='1')?SIGUSR1:SIGUSR2;
            for(auto&s:slots) if(s.pid>0) kill(s.pid,sig);
        }
    }
}

