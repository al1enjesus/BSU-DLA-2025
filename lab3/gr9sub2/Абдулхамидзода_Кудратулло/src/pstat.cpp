#include <bits/stdc++.h>
#include <sys/types.h>
#include <unistd.h>

using namespace std;

static string read_all(const string &path) {
    ifstream f(path);
    if (!f) return string();
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static vector<string> split_ws(const string &s) {
    vector<string> out;
    istringstream iss(s);
    string tok;
    while (iss >> tok) out.push_back(tok);
    return out;
}

static string trim(const string &s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

static bool parse_status_map(const string &content, unordered_map<string,string> &mapout) {
    if (content.empty()) return false;
    istringstream iss(content);
    string line;
    while (getline(iss, line)) {
        auto pos = line.find(':');
        if (pos == string::npos) continue;
        string key = trim(line.substr(0,pos));
        string val = trim(line.substr(pos+1));
        mapout[key] = val;
    }
    return true;
}

static long long parse_kb_value(const string &val_with_unit) {
    istringstream iss(val_with_unit);
    long long x=0;
    if (!(iss >> x)) return 0;
    return x;
}

static string format_bytes_binary(long long bytes) {
    const long long Ki = 1024LL;
    const long long Mi = Ki*Ki;
    const long long Gi = Mi*Ki;
    std::ostringstream out;
    out.setf(std::ios::fixed);
    out<<setprecision(2);
    if (bytes >= Gi) out << (double)bytes / Gi << " GiB";
    else if (bytes >= Mi) out << (double)bytes / Mi << " MiB";
    else if (bytes >= Ki) out << (double)bytes / Ki << " KiB";
    else out << bytes << " B";
    return out.str();
}

int main(int argc, char **argv) {
    if (argc != 2) {
        cerr << "Usage: " << argv[0] << " <pid>\n";
        return 2;
    }
    string pid = argv[1];

    string proc_stat_path = "/proc/" + pid + "/stat";
    string proc_status_path = "/proc/" + pid + "/status";
    string proc_io_path = "/proc/" + pid + "/io";
    string proc_smaps_rollup_path = "/proc/" + pid + "/smaps_rollup";

    string stat_content = read_all(proc_stat_path);
    string status_content = read_all(proc_status_path);
    string io_content = read_all(proc_io_path);
    string smaps_content = read_all(proc_smaps_rollup_path);

    if (stat_content.empty() && status_content.empty()) {
        cerr << "Can't read /proc/" << pid << " (process may not exist or permission denied)\n";
        return 1;
    }

    size_t pos_rparen = stat_content.rfind(')');
    if (pos_rparen == string::npos) {
        cerr << "Malformed stat file\n";
        return 1;
    }
    string after = stat_content.substr(pos_rparen + 1);
    vector<string> toks = split_ws(after);
    unsigned long long utime = 0, stime = 0;
    if (toks.size() >= 13) {
        try {
            utime = stoull(toks[11]);
            stime = stoull(toks[12]);
        } catch (...) {
            utime = stime = 0;
        }
    }

    unordered_map<string,string> status_map;
    parse_status_map(status_content, status_map);

    string ppid = status_map.count("PPid") ? status_map["PPid"] : "N/A";
    string threads = status_map.count("Threads") ? status_map["Threads"] : "N/A";
    string state = status_map.count("State") ? status_map["State"] : "N/A";
    string vmrss = status_map.count("VmRSS") ? status_map["VmRSS"] : "N/A";
    string voluntary = status_map.count("voluntary_ctxt_switches") ? status_map["voluntary_ctxt_switches"] : "N/A";
    string nonvoluntary = status_map.count("nonvoluntary_ctxt_switches") ? status_map["nonvoluntary_ctxt_switches"] : "N/A";

    unordered_map<string,string> io_map;
    parse_status_map(io_content, io_map);
    string read_bytes_s = io_map.count("read_bytes") ? io_map["read_bytes"] : "N/A";
    string write_bytes_s = io_map.count("write_bytes") ? io_map["write_bytes"] : "N/A";

    unordered_map<string,string> smapm;
    parse_status_map(smaps_content, smapm);
    string rss_anon = "";
    string rss_file = "";
    if (!smaps_content.empty()) {
        if (smapm.count("RssAnon")) rss_anon = smapm["RssAnon"];
        else if (smapm.count("RssAnon:")) rss_anon = smapm["RssAnon:"];
        if (smapm.count("RssFile")) rss_file = smapm["RssFile"];
        else if (smapm.count("RssFile:")) rss_file = smapm["RssFile:"];
    }

    long long vmrss_kb = vmrss == "N/A" ? 0 : parse_kb_value(vmrss);
    long long rss_bytes = vmrss_kb * 1024LL;
    long long read_bytes = 0, write_bytes = 0;
    try { if (read_bytes_s != "N/A") read_bytes = stoll(read_bytes_s); } catch(...) { read_bytes = 0; }
    try { if (write_bytes_s != "N/A") write_bytes = stoll(write_bytes_s); } catch(...) { write_bytes = 0; }

    long clk_tck = sysconf(_SC_CLK_TCK);
    double cpu_time_sec = (double)(utime + stime) / (double)clk_tck;

    cout << "pstat snapshot for pid: " << pid << "\n";
    cout << "----------------------------------------\n";
    cout << "PPid: " << ppid << "\n";
    cout << "Threads: " << threads << "\n";
    cout << "State: " << state << "\n\n";

    cout << "utime (jiffies): " << utime << "\n";
    cout << "stime (jiffies): " << stime << "\n";
    cout << "CLK_TCK (HZ): " << clk_tck << "\n";
    cout << fixed << setprecision(6);
    cout << "CPU time sec = (utime+stime)/HZ = (" << (utime+stime) << ")/" << clk_tck << " = " << cpu_time_sec << " s\n\n";

    cout << "voluntary_ctxt_switches: " << voluntary << "\n";
    cout << "nonvoluntary_ctxt_switches: " << nonvoluntary << "\n\n";

    cout << "VmRSS (raw): " << vmrss << "\n";
    cout << "RSS = " << format_bytes_binary(rss_bytes) << "\n";
    if (!rss_anon.empty()) {
        cout << "RssAnon: " << rss_anon << "\n";
    } else {
        cout << "RssAnon: N/A (no smaps_rollup or field missing)\n";
    }
    if (!rss_file.empty()) {
        cout << "RssFile: " << rss_file << "\n";
    } else {
        cout << "RssFile: N/A (no smaps_rollup or field missing)\n";
    }
    cout << "\n";

    cout << "read_bytes (from /proc/" << pid << "/io): " << read_bytes << " (" << format_bytes_binary(read_bytes) << ")\n";
    cout << "write_bytes (from /proc/" << pid << "/io): " << write_bytes << " (" << format_bytes_binary(write_bytes) << ")\n";

    cout << "----------------------------------------\n";
    return 0;
}
