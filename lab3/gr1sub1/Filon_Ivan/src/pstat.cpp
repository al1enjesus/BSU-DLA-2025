#include <bits/stdc++.h>
#include <unistd.h>
#include <sys/types.h>
using namespace std;

static string proc_path(pid_t pid, const string &file){
    return "/proc/" + to_string(pid) + "/" + file;
}

static optional<string> read_file_all(const string &path){
    ifstream in(path);
    if(!in) return nullopt;
    stringstream ss; ss << in.rdbuf();
    return ss.str();
}

static vector<string> split_ws(const string &s){
    vector<string> out;
    istringstream iss(s);
    string t;
    while(iss >> t) out.push_back(t);
    return out;
}

static optional<long long> parse_kb_line(const string &line, const string &key){
    if(line.rfind(key,0) != 0) return nullopt;
    for(size_t i=key.size(); i<line.size(); ++i){
        if(isdigit((unsigned char)line[i])){
            string num = line.substr(i);
            size_t j=0; while(j<num.size() && isdigit((unsigned char)num[j])) ++j;
            num = num.substr(0,j);
            try{ return stoll(num); } catch(...) { return nullopt; }
        }
    }
    return nullopt;
}

static string human_bytes(long long bytes){
    const long long Ki = 1024;
    const long long Mi = Ki*1024;
    ostringstream oss;
    if(bytes >= Mi){
        double v = (double)bytes / (double)Mi;
        oss.setf(ios::fixed); oss.precision(2);
        oss << v << " MiB";
    } else if(bytes >= Ki){
        double v = (double)bytes / (double)Ki;
        oss.setf(ios::fixed); oss.precision(1);
        oss << v << " KiB";
    } else {
        oss << bytes << " B";
    }
    return oss.str();
}

int main(int argc, char **argv){
    if(argc != 2){
        cerr << "Usage: pstat <pid>\n";
        return 2;
    }
    pid_t pid = 0;
    try{ pid = stoi(argv[1]); } catch(...){ cerr<<"Invalid pid\n"; return 2; }

    auto stat_raw = read_file_all(proc_path(pid,"stat"));
    if(!stat_raw){ cerr << "Failed to open "<< proc_path(pid,"stat") << "\n"; return 1; }
    string stat = *stat_raw;
    size_t lp = stat.find('(');
    size_t rp = stat.rfind(')');
    string comm = "";
    string after;
    string state;
    long long utime_ticks = 0, stime_ticks = 0;
    long long ppid = 0;
    if(lp != string::npos && rp != string::npos && rp > lp){
        comm = stat.substr(lp+1, rp-lp-1);
        after = stat.substr(rp+2);
        auto toks = split_ws(after);
        if(toks.size() >= 15){
            state = toks[0];
            try{ ppid = stoll(toks[1]); } catch(...) { ppid = 0; }
            if(toks.size() > 12){
                try{ utime_ticks = stoll(toks[11]); } catch(...) { utime_ticks = 0; }
            }
            if(toks.size() > 13){
                try{ stime_ticks = stoll(toks[12]); } catch(...) { stime_ticks = 0; }
            }
        }
    }

    auto status_raw = read_file_all(proc_path(pid,"status"));
    long long vmrss_kb = -1;
    long long threads = -1;
    long long voluntary = -1, nonvoluntary = -1;
    string state_from_status;
    if(status_raw){
        istringstream iss(*status_raw);
        string line;
        while(getline(iss,line)){
            if(line.rfind("VmRSS:",0)==0){
                auto v = parse_kb_line(line, "VmRSS:"); if(v) vmrss_kb = *v;
            } else if(line.rfind("Threads:",0)==0){
                auto v = parse_kb_line(line, "Threads:"); if(v) threads = *v;
            } else if(line.rfind("State:",0)==0){
                state_from_status = line.substr(string("State:").size());
                while(!state_from_status.empty() && isspace((unsigned char)state_from_status.front())) state_from_status.erase(state_from_status.begin());
            } else if(line.rfind("voluntary_ctxt_switches:",0)==0){
                auto v = parse_kb_line(line, "voluntary_ctxt_switches:"); if(v) voluntary = *v;
            } else if(line.rfind("nonvoluntary_ctxt_switches:",0)==0){
                auto v = parse_kb_line(line, "nonvoluntary_ctxt_switches:"); if(v) nonvoluntary = *v;
            }
        }
    }

    auto io_raw = read_file_all(proc_path(pid,"io"));
    long long read_bytes = -1, write_bytes = -1;
    if(io_raw){
        istringstream iss(*io_raw);
        string line;
        while(getline(iss,line)){
            if(line.rfind("read_bytes:",0)==0){
                auto v = parse_kb_line(line, "read_bytes:"); if(v) read_bytes = *v;
            } else if(line.rfind("write_bytes:",0)==0){
                auto v = parse_kb_line(line, "write_bytes:"); if(v) write_bytes = *v;
            }
        }
    }

    auto sr_raw = read_file_all(proc_path(pid, "smaps_rollup"));
    long long rss_anon_kb = -1, rss_file_kb = -1;
    if(sr_raw){
        istringstream iss(*sr_raw);
        string line;
        while(getline(iss,line)){
            if(line.rfind("RssAnon:",0)==0){
                auto v = parse_kb_line(line, "RssAnon:"); if(v) rss_anon_kb = *v;
            } else if(line.rfind("RssFile:",0)==0){
                auto v = parse_kb_line(line, "RssFile:"); if(v) rss_file_kb = *v;
            }
        }
    }

    long hz = sysconf(_SC_CLK_TCK);
    double cpu_time_sec = ((double)utime_ticks + (double)stime_ticks) / (double)hz;

    cout << "pstat: pid="<< pid <<" ("<< comm <<")\n";
    cout << "------------------------------------------------\n";
    cout << left << setw(28) << "PPid:" << ppid << "\n";
    if(state.size()) cout << left << setw(28) << "State (stat):" << state << "\n";
    if(!state_from_status.empty()) cout << left << setw(28) << "State (status):" << state_from_status << "\n";
    if(threads >= 0) cout << left << setw(28) << "Threads:" << threads << "\n";
    cout << left << setw(28) << "utime (ticks):" << utime_ticks << "\n";
    cout << left << setw(28) << "stime (ticks):" << stime_ticks << "\n";
    cout << left << setw(28) << "CPU time sec = (utime+stime)/HZ:";
    cout.setf(ios::fixed); cout<<setprecision(4)<< cpu_time_sec << "  (HZ="<< hz <<")\n";
    if(voluntary >= 0) cout << left << setw(28) << "voluntary_ctxt_switches:" << voluntary << "\n";
    if(nonvoluntary >= 0) cout << left << setw(28) << "nonvoluntary_ctxt_switches:" << nonvoluntary << "\n";
    if(vmrss_kb >= 0){
        long long vmrss_bytes = vmrss_kb * 1024LL;
        cout << left << setw(28) << "VmRSS:" << vmrss_kb << " kB ("<< human_bytes(vmrss_bytes) <<")" << "\n";
        double rss_mib = (double)vmrss_kb / 1024.0;
        cout << left << setw(28) << "RSS (MiB):";
        cout.setf(ios::fixed); cout<<setprecision(3)<< rss_mib << " MiB\n";
    }
    if(rss_anon_kb >= 0) cout << left << setw(28) << "RssAnon:" << rss_anon_kb << " kB ("<< human_bytes(rss_anon_kb*1024LL) <<")\n";
    if(rss_file_kb >= 0) cout << left << setw(28) << "RssFile:" << rss_file_kb << " kB ("<< human_bytes(rss_file_kb*1024LL) <<")\n";
    if(read_bytes >= 0) cout << left << setw(28) << "read_bytes:" << read_bytes << "\n";
    if(write_bytes >= 0) cout << left << setw(28) << "write_bytes:" << write_bytes << "\n";
    cout << "------------------------------------------------\n";
    return 0;
}
