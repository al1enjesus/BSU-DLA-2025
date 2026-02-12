#include <iostream>
#include <fstream>
#include <sstream>
#include <csignal>
#include <unistd.h>
#include <sys/wait.h>
#include <vector>
#include <map>
#include <chrono>
#include <deque>
#include <algorithm>
#include <yaml-cpp/yaml.h>
#include <atomic>
#include <mutex>
#include <sched.h>
#include <sys/resource.h>
#include <cstring>

using namespace std;

class Supervisor {
public:
	struct Mode {
		int work_us;
		int sleep_us;
	};

	struct Config {
		int workers;
		Mode heavy;
		Mode light;
		int scheduling_policy;
	};

	enum class WorkMode { LIGHT = 1, HEAVY = 2 };
	enum class SchedulingPolicy { DEFAULT, MIXED_NICE, CPU_AFFINITY };

private:
	static constexpr int SHUTDOWN_TIMEOUT_SECONDS = 5;
	static constexpr int RESTART_CHECK_WINDOW_SECONDS = 30;
	static constexpr int MAX_RESTARTS_PER_WINDOW = 5;
	static constexpr int STATUS_CHECK_INTERVAL_US = 200000;
	static constexpr int DEFAULT_WORKERS = 3;
	static constexpr int DEFAULT_HEAVY_WORK_US = 5000000;
	static constexpr int DEFAULT_HEAVY_SLEEP_US = 1000000;
	static constexpr int DEFAULT_LIGHT_WORK_US = 1000000;
	static constexpr int DEFAULT_LIGHT_SLEEP_US = 500000;
	static constexpr int NICE_HIGH = 10;
	static constexpr int NICE_NORMAL = 0;
	static constexpr int DEFAULT_CPU_AFFINITIES = 1;

	static inline Config cfg{};
	static inline vector<pid_t> workers{};
	static inline atomic<bool> terminating{false};
	static inline atomic<bool> reload{false};
	static inline atomic<bool> shuttingDown{false};
	static inline atomic<WorkMode> mode{WorkMode::HEAVY};
	static inline map<pid_t, int> workerIndex{};
	static inline deque<chrono::steady_clock::time_point> restartTimes{};
	static inline string cfgFile{};
	static inline string workerExec{};
	static inline mutex workerMutex{};
	static inline mutex restartMutex{};
	static inline mutex indexMutex{};
	static inline SchedulingPolicy schedPolicy{SchedulingPolicy::DEFAULT};
	static inline int cpuCount{1};
	static inline int nextIndex{0};

	Supervisor();

	static void readConfig() {
		try {
			YAML::Node yaml = YAML::LoadFile(cfgFile);
			cfg.workers = yaml["workers"].as<int>();
			cfg.heavy = {yaml["heavy"]["work_us"].as<int>(), yaml["heavy"]["sleep_us"].as<int>()};
			cfg.light = {yaml["light"]["work_us"].as<int>(), yaml["light"]["sleep_us"].as<int>()};
			cfg.scheduling_policy = yaml["scheduling_policy"].as<int>(0);
			setSchedulingPolicy(cfg.scheduling_policy);
		} catch (const YAML::Exception &e) {
			cerr << "[SUP] YAML error: " << e.what() << endl;
			cfg.workers = DEFAULT_WORKERS;
			cfg.heavy = {DEFAULT_HEAVY_WORK_US, DEFAULT_HEAVY_SLEEP_US};
			cfg.light = {DEFAULT_LIGHT_WORK_US, DEFAULT_LIGHT_SLEEP_US};
			cfg.scheduling_policy = 0;
		}
	}

	static void setupSystemInfo() {
		cpu_set_t set;
		CPU_ZERO(&set);
		if (sched_getaffinity(0, sizeof(set), &set) == 0) {
			cpuCount = CPU_COUNT(&set);
			cout << "[SUP] CPUs available: " << cpuCount << endl;
		} else {
			cerr << "[SUP] Can't get CPU info: " << strerror(errno) << endl;
			cpuCount = DEFAULT_CPU_AFFINITIES;
		}
	}

	static void applySchedulingPolicy(int index) {
		switch (schedPolicy) {
			case SchedulingPolicy::MIXED_NICE: {
				int niceVal = (index % 2 == 0) ? NICE_HIGH : NICE_NORMAL;
				if (setpriority(PRIO_PROCESS, 0, niceVal) != 0)
					cerr << "[WORKER] Failed to set nice=" << niceVal << ": " << strerror(errno) << endl;
				else
					cout << "[WORKER] Nice=" << niceVal << endl;
				break;
			}
			case SchedulingPolicy::CPU_AFFINITY: {
				if (cpuCount > 1) {
					cpu_set_t cpuset;
					CPU_ZERO(&cpuset);
					int target = index % cpuCount;
					CPU_SET(target, &cpuset);
					if (sched_setaffinity(0, sizeof(cpuset), &cpuset) != 0)
						cerr << "[WORKER] Failed affinity " << target << ": " << strerror(errno) << endl;
					else
						cout << "[WORKER] Affinity -> CPU " << target << endl;
				} else {
					cerr << "[WORKER] Not enough CPUs for affinity\n";
				}
				break;
			}
			default:
				break;
		}
	}

	static void spawnWorker(int index) {
		pid_t pid = fork();
		if (pid < 0) {
			cerr << "[SUP] Fork failed\n";
			return;
		}
		if (pid == 0) {
			applySchedulingPolicy(index);
			string args[] = {
				to_string(cfg.heavy.work_us),
				to_string(cfg.heavy.sleep_us),
				to_string(cfg.light.work_us),
				to_string(cfg.light.sleep_us),
				to_string(static_cast<int>(mode.load()))
			};
			execl(workerExec.c_str(), workerExec.c_str(),
				args[0].c_str(), args[1].c_str(),
				args[2].c_str(), args[3].c_str(),
				args[4].c_str(), nullptr);
			_exit(1);
		}
		{
			lock_guard<mutex> lock(workerMutex);
			workers.push_back(pid);
		}
		{
			lock_guard<mutex> lock(indexMutex);
			workerIndex[pid] = index;
		}
		cout << "[SUP] Worker " << pid << " started (index " << index << ")\n";
	}

	static void stopWorkers() {
		{
			lock_guard<mutex> lock(workerMutex);
			for (pid_t pid : workers) kill(pid, SIGTERM);
		}

		auto deadline = chrono::steady_clock::now() + chrono::seconds(SHUTDOWN_TIMEOUT_SECONDS);
		while (true) {
			bool empty;
			{
				lock_guard<mutex> lock(workerMutex);
				empty = workers.empty();
			}
			if (empty || chrono::steady_clock::now() >= deadline) break;
			int status;
			pid_t pid = waitpid(-1, &status, WNOHANG);
			if (pid > 0) {
				lock_guard<mutex> lock(workerMutex);
				workers.erase(remove(workers.begin(), workers.end(), pid), workers.end());
			} else {
				usleep(STATUS_CHECK_INTERVAL_US);
			}
		}

		{
			lock_guard<mutex> lock(workerMutex);
			for (pid_t pid : workers) {
				cout << "[SUP] Forcing kill " << pid << endl;
				kill(pid, SIGKILL);
			}
		}
		int status;
		while (waitpid(-1, &status, 0) > 0) {}
		lock_guard<mutex> lock(workerMutex);
		workers.clear();
	}

	static void onSigchld(int) {
		int status;
		pid_t pid;
		while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
			int idx = -1;
			{
				lock_guard<mutex> lock(indexMutex);
				auto it = workerIndex.find(pid);
				if (it != workerIndex.end()) {
					idx = it->second;
					workerIndex.erase(it);
				}
			}
			cout << "[SUP] Worker " << pid << " (index " << idx << ") exited\n";
			{
				lock_guard<mutex> lock(workerMutex);
				workers.erase(remove(workers.begin(), workers.end(), pid), workers.end());
			}
			if (!shuttingDown && !reload) {
				auto now = chrono::steady_clock::now();
				lock_guard<mutex> lock(restartMutex);
				restartTimes.push_back(now);
				while (!restartTimes.empty() &&
					   chrono::duration_cast<chrono::seconds>(now - restartTimes.front()).count() > RESTART_CHECK_WINDOW_SECONDS)
					restartTimes.pop_front();
				if ((int)restartTimes.size() <= MAX_RESTARTS_PER_WINDOW) {
					int newIndex = (idx != -1) ? idx : nextIndex++;
					spawnWorker(newIndex);
				} else {
					cerr << "[SUP] Too many restarts, skipping respawn\n";
				}
			}
		}
	}

	static void onTerminate(int) {
		terminating = true;
		shuttingDown = true;
	}

	static void onReload(int) { reload = true; }

	static void onLight(int) {
		cout << "[SUP] Switching to LIGHT\n";
		mode = WorkMode::LIGHT;
		lock_guard<mutex> lock(workerMutex);
		for (pid_t pid : workers) kill(pid, SIGUSR1);
	}

	static void onHeavy(int) {
		cout << "[SUP] Switching to HEAVY\n";
		mode = WorkMode::HEAVY;
		lock_guard<mutex> lock(workerMutex);
		for (pid_t pid : workers) kill(pid, SIGUSR2);
	}

	static void setupSignals() {
		signal(SIGCHLD, onSigchld);
		signal(SIGTERM, onTerminate);
		signal(SIGINT, onTerminate);
		signal(SIGHUP, onReload);
		signal(SIGUSR1, onLight);
		signal(SIGUSR2, onHeavy);
	}

	static void setSchedulingPolicy(int p) {
		switch (p) {
			case 1:
				schedPolicy = SchedulingPolicy::MIXED_NICE;
				cout << "[SUP] Policy: Mixed nice\n";
				break;
			case 2:
				schedPolicy = SchedulingPolicy::CPU_AFFINITY;
				cout << "[SUP] Policy: CPU affinity\n";
				break;
			default:
				schedPolicy = SchedulingPolicy::DEFAULT;
				cout << "[SUP] Policy: Default\n";
				break;
		}
	}

public:
	static void setup(const string &cfgPath, const string &workerPath) {
		cfgFile = cfgPath;
		workerExec = workerPath;
		setupSignals();
		setupSystemInfo();
		readConfig();
	}

	static void run() {
		nextIndex = cfg.workers;
		for (int i = 0; i < cfg.workers; i++) spawnWorker(i);
		while (true) {
			if (terminating) {
				cout << "[SUP] Shutdown\n";
				stopWorkers();
				break;
			}
			if (reload) {
				cout << "[SUP] Reloading config\n";
				readConfig();
				stopWorkers();
				for (int i = 0; i < cfg.workers; i++) spawnWorker(i);
				reload = false;
			}
			pause();
		}
	}
};

int main(int argc, char *argv[]) {
	if (argc < 3) {
		cerr << "Usage: " << argv[0] << " <config_file> <worker_path>\n";
		return 1;
	}
	string cfgPath = argv[1];
	string workerPath = argv[2];
	Supervisor::setup(cfgPath, workerPath);
	Supervisor::run();
	return 0;
}
