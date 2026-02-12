#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <unistd.h>
#include <map>
#include <algorithm>
#include <cctype>
#include <stdexcept>

using namespace std;

string trim(const string &s)
{
  auto start = find_if_not(s.begin(),
                           s.end(),
                           [](unsigned char ch)
                           { return isspace(ch); });
  auto end = find_if_not(s.rbegin(),
                         s.rend(),
                         [](unsigned char ch)
                         { return isspace(ch); })
                 .base();

  if (start >= end)
  {
    return "";
  }

  return string(start, end);
}

string format_bytes(long long bytes)
{
  if (bytes > 1024 * 1024)
  {
    return to_string(bytes / (1024 * 1024)) + " MiB";
  }

  if (bytes > 1024)
  {
    return to_string(bytes / 1024) + " KiB";
  }

  return to_string(bytes) + " B";
}

string get_value_from_file(const string &path, const string &key)
{
  ifstream file(path);
  string line;

  while (getline(file, line))
  {
    if (line.rfind(key, 0) == 0)
    {
      return trim(line.substr(line.find(":") + 1));
    }
  }

  return "N/A";
}

bool is_number(const string &str)
{
  return !str.empty() && all_of(str.begin(), str.end(), ::isdigit);
}

int main(int argc, char *argv[])
{
  if (argc != 2)
  {
    cerr << "Usage: " << argv[0] << " <pid>" << endl;
    return 1;
  }

  string pid = argv[1];

  if (!is_number(pid))
  {
    cerr << "Error: PID must be a valid number." << endl;
    return 1;
  }

  long hz = sysconf(_SC_CLK_TCK);

  string stat_path = "/proc/" + pid + "/stat";
  ifstream stat_file(stat_path);
  if (!stat_file)
  {
    cerr << "Error: Could not open /proc/" << pid << "/stat. Process not found?" << endl;
    return 1;
  }

  string stat_line;
  getline(stat_file, stat_line);

  char comm[256];
  char state;
  long ppid;
  long utime, stime;

  int parsed = sscanf(
      stat_line.c_str(),
      "%*d (%255[^)]) %c %ld %*s %*s %*s %*s %*s %*s %*s %*s %*s %*s %ld %ld",
      comm, &state, &ppid, &utime, &stime);

  if (parsed != 5)
  {
    cerr << "Error: failed to parse " << stat_path << endl;
    return 1;
  }

  string status_path = "/proc/" + pid + "/status";
  string threads_str = get_value_from_file(status_path, "Threads");
  string vmrss_str = get_value_from_file(status_path, "VmRSS");
  string voluntary_switches_str = get_value_from_file(status_path, "voluntary_ctxt_switches");
  string nonvoluntary_switches_str = get_value_from_file(status_path, "nonvoluntary_ctxt_switches");

  string io_path = "/proc/" + pid + "/io";
  string read_bytes_str = get_value_from_file(io_path, "read_bytes");
  string write_bytes_str = get_value_from_file(io_path, "write_bytes");

  string smaps_path = "/proc/" + pid + "/smaps_rollup";
  string rssanon_str = "N/A";
  string rssfile_str = "N/A";

  ifstream smaps_file(smaps_path);
  if (smaps_file)
  {
    rssanon_str = get_value_from_file(smaps_path, "RssAnon");
    rssfile_str = get_value_from_file(smaps_path, "RssFile");
  }

  int width = 30;

  try
  {
    cout << "--- pstat for PID: " << pid << " ---" << endl;
    cout << left << setw(width) << "PPid:" << ppid << endl;
    cout << left << setw(width) << "Threads:" << threads_str << endl;
    cout << left << setw(width) << "State:" << state << endl;

    double cpu_time_sec = (double)(utime + stime) / hz;
    cout << left << setw(width) << "utime/stime (ticks):" << utime << " / " << stime << endl;
    cout << left << setw(width) << "CPU time (sec):" << fixed << setprecision(2) << cpu_time_sec << endl;

    cout << left << setw(width) << "voluntary_ctxt_switches:" << voluntary_switches_str << endl;
    cout << left << setw(width) << "nonvoluntary_ctxt_switches:" << nonvoluntary_switches_str << endl;

    long vmrss_kb = vmrss_str != "N/A" ? stol(vmrss_str) : 0;
    cout << left << setw(width) << "VmRSS:" << vmrss_str << endl;
    cout << left << setw(width) << "RSS MiB:" << fixed << setprecision(2) << (double)vmrss_kb / 1024.0 << " MiB" << endl;

    cout << left << setw(width) << "RssAnon:" << rssanon_str << endl;
    cout << left << setw(width) << "RssFile:" << rssfile_str << endl;

    long long read_bytes = read_bytes_str != "N/A" ? stoll(read_bytes_str) : 0;
    long long write_bytes = write_bytes_str != "N/A" ? stoll(write_bytes_str) : 0;
    cout << left << setw(width) << "read_bytes:" << format_bytes(read_bytes) << endl;
    cout << left << setw(width) << "write_bytes:" << format_bytes(write_bytes) << endl;
  }
  catch (const exception &e)
  {
    cerr << "Invalid argument: " << e.what() << endl;
  }

  return 0;
}
