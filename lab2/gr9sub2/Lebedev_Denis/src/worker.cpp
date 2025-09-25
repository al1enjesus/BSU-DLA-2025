#include <iostream>
#include <unistd.h>
#include <csignal>
#include <atomic>
#include <chrono>
#include <sched.h>

using namespace std;

class Worker
{
public:
	struct Mode
	{
		int work_us;
		int sleep_us;
	};

	enum class WorkMode
	{
		LIGHT = 1,
		HEAVY = 2
	};

private:
	static inline atomic<bool> should_run{true};
	static inline atomic<WorkMode> mode{WorkMode::HEAVY};

	Mode heavy;
	Mode light;
	int tick;

	static void sigtermHandler(int) { should_run = false; }
	static void sigusr1Handler(int) { mode = WorkMode::LIGHT; }
	static void sigusr2Handler(int) { mode = WorkMode::HEAVY; }

	void performWork(const Mode &mode)
	{
		usleep(mode.work_us);	 // work
		usleep(mode.sleep_us); // wait
	}

	void printStatus()
	{
		tick++;

		int cpu = sched_getcpu();
		cout << "PID=" << getpid()
				 << " mode=" << (mode == WorkMode::HEAVY ? "HEAVY" : "LIGHT")
				 << " tick=" << tick
				 << " cpu=" << cpu
				 << endl;
	}

public:
	Worker(int heavy_work_us, int heavy_sleep_us,
				 int light_work_us, int light_sleep_us,
				 WorkMode initial_mode)
			: tick(0)
	{
		heavy = {heavy_work_us, heavy_sleep_us};
		light = {light_work_us, light_sleep_us};
		mode = initial_mode;
	}

	void setupSignalHandlers()
	{
		signal(SIGTERM, sigtermHandler);
		signal(SIGUSR1, sigusr1Handler);
		signal(SIGUSR2, sigusr2Handler);
	}

	void run()
	{
		setupSignalHandlers();

		while (should_run)
		{
			Mode currentMode = (mode == WorkMode::HEAVY ? heavy : light);
			performWork(currentMode);
			printStatus();
		}

		cout << "[WORKER " << getpid() << "] exiting\n";
	}
};

int main(int argc, char *argv[])
{
	if (argc < 6)
	{
		cerr << "Usage: " << argv[0] << " <heavy_work_us> <heavy_sleep_us> <light_work_us> <light_sleep_us> <mode>\n";
		return 1;
	}

	int heavyWork = stoi(argv[1]);
	int heavySleep = stoi(argv[2]);
	int lightWork = stoi(argv[3]);
	int lightSleep = stoi(argv[4]);

	int modeValue = stoi(argv[5]);
	auto initialMode = static_cast<Worker::WorkMode>(modeValue);

	if (heavyWork <= 0 || heavySleep <= 0 || lightWork <= 0 || lightSleep <= 0)
	{
		cerr << "All time parameters must be positive integers.\n";
		return 1;
	}

	Worker worker(heavyWork, heavySleep, lightWork, lightSleep, initialMode);
	worker.run();

	return 0;
}
