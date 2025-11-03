#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unistd.h>
#include <iomanip>
#include <cstdlib>

using namespace std;

long HZ = sysconf(_SC_CLK_TCK);

string format_bytes(long long bytes) {
    if (bytes > 1024 * 1024)
        return to_string(static_cast<double>(bytes) / (1024 * 1024)) + " MiB";
    else if (bytes > 1024)
        return to_string(static_cast<double>(bytes) / 1024) + " KiB";
    else
        return to_string(bytes) + " B";
}

string read_file(const string &path) {
    ifstream f(path);
    if (!f.is_open()) return "";
    stringstream buffer;
    buffer << f.rdbuf();
    return buffer.str();
}

unordered_map<string, string> parse_key_value_file(const string &path) {
    unordered_map<string, string> result;
    string content = read_file(path);
    if (content.empty()) return result;

    istringstream iss(content);
    string line;
    while (getline(iss, line)) {
        auto pos = line.find(':');
        if (pos != string::npos) {
            string key = line.substr(0, pos);
            string value = line.substr(pos + 1);
            while (!key.empty() && isspace(key.back())) key.pop_back();
            while (!value.empty() && isspace(value.front())) value.erase(value.begin());
            result[key] = value;
        }
    }
    return result;
}

struct StatInfo {
    int ppid = 0;
    string state;
    long utime = 0;
    long stime = 0;
};

StatInfo parse_stat(const string &path) {
    StatInfo s;
    ifstream f(path);
    if (!f.is_open()) return s;
    string line;
    getline(f, line);
    string comm;
    istringstream iss(line);
    string tmp;
    iss >> tmp;
    iss >> comm;
    iss >> s.state;
    iss >> s.ppid;
    for (int i = 0; i < 9; ++i) iss >> tmp;
    iss >> s.utime >> s.stime;
    return s;
}

unordered_map<string, long long> parse_io(const string &path) {
    unordered_map<string, long long> result;
    string content = read_file(path);
    if (content.empty()) return result;

    istringstream iss(content);
    string line;
    while (getline(iss, line)) {
        auto pos = line.find(':');
        if (pos != string::npos) {
            string key = line.substr(0, pos);
            string value = line.substr(pos + 1);
            while (!value.empty() && isspace(value.front())) value.erase(value.begin());
            try {
                result[key] = stoll(value);
            } catch (...) {}
        }
    }
    return result;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        cerr << "Usage: pstat <pid>" << endl;
        return 1;
    }
    string pid = argv[1];
    string base = "/proc/" + pid + "/";

    StatInfo stat = parse_stat(base + "stat");
    auto status = parse_key_value_file(base + "status");
    auto io = parse_io(base + "io");
    auto smaps = parse_key_value_file(base + "smaps_rollup");

    if (stat.state.empty() || status.empty()) {
        cerr << "Process " << pid << " not found or permission denied." << endl;
        return 1;
    }

    double cpu_time_sec = (stat.utime + stat.stime) / static_cast<double>(HZ);

    long vmrss_kb = 0;
    if (status.count("VmRSS")) {
        istringstream(status["VmRSS"]) >> vmrss_kb;
    }

    long rssAnon_kb = 0, rssFile_kb = 0;
    if (status.count("RssAnon")) istringstream(status["RssAnon"]) >> rssAnon_kb;
    if (status.count("RssFile")) istringstream(status["RssFile"]) >> rssFile_kb;

    cout << "PID: " << pid << endl;
    cout << "PPid: " << stat.ppid << endl;
    cout << "State: " << stat.state << endl;
    cout << "Threads: " << status["Threads"] << endl;
    cout << fixed << setprecision(2);
    cout << "CPU time: " << cpu_time_sec << " sec" << endl;
    cout << "VmRSS: " << format_bytes(vmrss_kb * 1024) << endl;
    cout << "RssAnon: " << rssAnon_kb << " kB" << endl;
    cout << "RssFile: " << rssFile_kb << " kB" << endl;
    cout << "voluntary_ctxt_switches: " << status["voluntary_ctxt_switches"] << endl;
    cout << "nonvoluntary_ctxt_switches: " << status["nonvoluntary_ctxt_switches"] << endl;

    if (io.count("read_bytes"))
        cout << "read_bytes: " << format_bytes(io["read_bytes"]) << endl;
    if (io.count("write_bytes"))
        cout << "write_bytes: " << format_bytes(io["write_bytes"]) << endl;
    if (smaps.count("Rss"))
        cout << "(from smaps_rollup) Rss: " << smaps["Rss"] << endl;

    return 0;
}
