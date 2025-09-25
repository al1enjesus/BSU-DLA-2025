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

using namespace std;

class Supervisor
{
public:
	struct Mode
	{
		int work_us;
		int sleep_us;
	};

	struct Config
	{
		int workers;
		Mode heavy;
		Mode light;
	};

	enum class WorkMode
	{
		LIGHT = 1,
		HEAVY = 2
	};

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

	static inline Config config{};
	static inline vector<pid_t> workers{};
	static inline atomic<bool> terminate{false};
	static inline atomic<bool> reload{false};
	static inline atomic<bool> shuttingDown{false};
	static inline atomic<WorkMode> currentMode{WorkMode::HEAVY};
	static inline map<pid_t, string> workerStatus{};
	static inline deque<chrono::steady_clock::time_point> restartTimes{};
	static inline string configFile{};
	static inline string workerPath{};
	static inline mutex workersMutex{};
	static inline mutex restartTimesMutex{};

	Supervisor();

	static void readConfig()
	{
		try
		{
			YAML::Node yamlConfig = YAML::LoadFile(configFile);

			config.workers = yamlConfig["workers"].as<int>();

			auto heavy = yamlConfig["heavy"];
			config.heavy.work_us = heavy["work_us"].as<int>();
			config.heavy.sleep_us = heavy["sleep_us"].as<int>();

			auto light = yamlConfig["light"];
			config.light.work_us = light["work_us"].as<int>();
			config.light.sleep_us = light["sleep_us"].as<int>();
		}
		catch (const YAML::Exception &e)
		{
			cerr << "[SUP] Error reading YAML config: " << e.what() << endl;

			// Use default values
			config.workers = DEFAULT_WORKERS;
			config.heavy = {DEFAULT_HEAVY_WORK_US, DEFAULT_HEAVY_SLEEP_US};
			config.light = {DEFAULT_LIGHT_WORK_US, DEFAULT_LIGHT_SLEEP_US};
		}
	}

	static void spawnWorker()
	{
		pid_t pid = fork();

		if (pid < 0)
		{
			cerr << "[SUP] Failed to fork worker process" << endl;
			return;
		}

		// Child process
		if (pid == 0)
		{
			// Pass config parameters as command line arguments
			string heavyWork = to_string(config.heavy.work_us);
			string heavySleep = to_string(config.heavy.sleep_us);
			string lightWork = to_string(config.light.work_us);
			string lightSleep = to_string(config.light.sleep_us);
			string workMode = to_string(static_cast<int>(currentMode.load()));

			execl(workerPath.c_str(), workerPath.c_str(),
						heavyWork.c_str(), heavySleep.c_str(),
						lightWork.c_str(), lightSleep.c_str(),
						workMode.c_str(),
						nullptr);
			_exit(1);
		}

		// Parent process
		{
			lock_guard<mutex> lock(workersMutex);
			workers.push_back(pid);
		}

		cout << "[SUP] started worker " << pid << endl;
	}

	static void stopWorkers()
	{
		{
			lock_guard<mutex> lock(workersMutex);
			for (pid_t pid : workers)
			{
				kill(pid, SIGTERM);
			}
		}

		auto deadline = chrono::steady_clock::now() + chrono::seconds(SHUTDOWN_TIMEOUT_SECONDS);
		while (true)
		{
			bool workersEmpty;
			{
				lock_guard<mutex> lock(workersMutex);
				workersEmpty = workers.empty();
			}

			if (workersEmpty || chrono::steady_clock::now() >= deadline)
				break;

			int status;
			pid_t pid = waitpid(-1, &status, WNOHANG);
			if (pid > 0)
			{
				lock_guard<mutex> lock(workersMutex);
				workers.erase(std::remove(workers.begin(), workers.end(), pid), workers.end());
			}
			else
			{
				usleep(STATUS_CHECK_INTERVAL_US);
			}
		}

		{
			lock_guard<mutex> lock(workersMutex);
			for (pid_t pid : workers)
			{
				cout << "[SUP] worker " << pid << " did not exit, killing\n";
				kill(pid, SIGKILL);
			}
		}

		int status;
		while (waitpid(-1, &status, 0) > 0)
		{
		}

		{
			lock_guard<mutex> lock(workersMutex);
			workers.clear();
		}
	}

	static void onSigchld(int)
	{
		int status;
		pid_t pid;

		while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
		{
			cout << "[SUP] worker " << pid << " exited" << endl;
			{
				lock_guard<mutex> lock(workersMutex);
				workers.erase(std::remove(workers.begin(), workers.end(), pid), workers.end());
			}

			if (!shuttingDown.load() && !reload.load())
			{
				auto now = chrono::steady_clock::now();
				{
					lock_guard<mutex> lock(restartTimesMutex);
					restartTimes.push_back(now);
					while (!restartTimes.empty() &&
								 chrono::duration_cast<chrono::seconds>(now - restartTimes.front()).count() > RESTART_CHECK_WINDOW_SECONDS)
					{
						restartTimes.pop_front();
					}

					if ((int)restartTimes.size() <= MAX_RESTARTS_PER_WINDOW)
					{
						spawnWorker();
					}
					else
					{
						cerr << "[SUP] too many restarts, not respawning for now\n";
					}
				}
			}
		}
	}

	static void onTerminate(int sig)
	{
		terminate.store(true);
		shuttingDown.store(true);
	}

	static void onReload(int)
	{
		reload.store(true);
	}

	static void onSwitchToLight(int)
	{
		cout << "[SUP] switching workers to LIGHT\n";
		currentMode.store(WorkMode::LIGHT);

		{
			lock_guard<mutex> lock(workersMutex);
			for (pid_t pid : workers)
			{
				kill(pid, SIGUSR1);
			}
		}
	}

	static void onSwitchToHeavy(int)
	{
		cout << "[SUP] switching workers to HEAVY\n";
		currentMode.store(WorkMode::HEAVY);

		{
			lock_guard<mutex> lock(workersMutex);
			for (pid_t pid : workers)
			{
				kill(pid, SIGUSR2);
			}
		}
	}

	static void setupSignalHandlers()
	{
		signal(SIGCHLD, onSigchld);
		signal(SIGTERM, onTerminate);
		signal(SIGINT, onTerminate);
		signal(SIGHUP, onReload);
		signal(SIGUSR1, onSwitchToLight);
		signal(SIGUSR2, onSwitchToHeavy);
	}

public:
	static void setup(const string &config_file, const string &worker_path)
	{
		configFile = config_file;
		workerPath = worker_path;
		setupSignalHandlers();
		readConfig();
	}

	static void run()
	{
		for (int i = 0; i < config.workers; i++)
		{
			spawnWorker();
		}

		while (true)
		{
			if (terminate.load())
			{
				cout << "[SUP] shutting down...\n";
				stopWorkers();
				break;
			}

			if (reload.load())
			{
				cout << "[SUP] reload config\n";
				readConfig();
				stopWorkers();

				for (int i = 0; i < config.workers; i++)
				{
					spawnWorker();
				}

				reload.store(false);
			}
			pause();
		}
	}
};

int main(int argc, char *argv[])
{
	string configFile;
	string workerPath;

	if (argc < 3)
	{
		cerr << "Usage: " << argv[0] << " <config_file> <worker_path>" << endl;
		return 1;
	}

	configFile = argv[1];
	workerPath = argv[2];

	if (configFile.empty() || workerPath.empty())
	{
		cerr << "Both config file and worker path must be provided." << endl;
		return 1;
	}

	Supervisor::setup(configFile, workerPath);
	Supervisor::run();

	return 0;
}