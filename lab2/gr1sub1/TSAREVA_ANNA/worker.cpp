#include <iostream>
#include <unistd.h>
#include <csignal>
#include <atomic>
#include <sched.h>
#include <sys/resource.h>

using namespace std;

class Worker {
public:
	struct Mode {
		int work_us;
		int sleep_us;
	};

	enum class WorkMode { LIGHT = 1, HEAVY = 2 };

private:
	static inline atomic<bool> active{true};
	static inline atomic<WorkMode> currentMode{WorkMode::HEAVY};

	Mode heavyCfg;
	Mode lightCfg;
	int counter;

	static void onSigTerm(int) { active = false; }
	static void onSigUsr1(int) { currentMode = WorkMode::LIGHT; }
	static void onSigUsr2(int) { currentMode = WorkMode::HEAVY; }

	void execute(const Mode& cfg) {
		usleep(cfg.work_us);
		usleep(cfg.sleep_us);
	}

	void showStatus() {
		counter++;
		int cpu = sched_getcpu();
		int niceValue = getpriority(PRIO_PROCESS, 0);

		cout << "PID=" << getpid()
		     << " mode=" << (currentMode == WorkMode::HEAVY ? "HEAVY" : "LIGHT")
		     << " tick=" << counter
		     << " cpu=" << cpu
		     << " nice=" << niceValue
		     << endl;
	}

	void initSignals() {
		signal(SIGTERM, onSigTerm);
		signal(SIGUSR1, onSigUsr1);
		signal(SIGUSR2, onSigUsr2);
	}

public:
	Worker(int hw, int hs, int lw, int ls, WorkMode startMode)
		: heavyCfg{hw, hs}, lightCfg{lw, ls}, counter(0) {
		currentMode = startMode;
	}

	void run() {
		initSignals();
		while (active) {
			Mode cfg = (currentMode == WorkMode::HEAVY ? heavyCfg : lightCfg);
			execute(cfg);
			showStatus();
		}
		cout << "[WORKER " << getpid() << "] exiting" << endl;
	}
};

int main(int argc, char* argv[]) {
	if (argc < 6) {
		cerr << "Usage: " << argv[0]
		     << " <heavy_work_us> <heavy_sleep_us> <light_work_us> <light_sleep_us> <mode>\n";
		return 1;
	}

	int hw = stoi(argv[1]);
	int hs = stoi(argv[2]);
	int lw = stoi(argv[3]);
	int ls = stoi(argv[4]);
	int modeVal = stoi(argv[5]);

	if (hw <= 0 || hs <= 0 || lw <= 0 || ls <= 0) {
		cerr << "All time parameters must be positive integers.\n";
		return 1;
	}

	auto mode = static_cast<Worker::WorkMode>(modeVal);
	Worker worker(hw, hs, lw, ls, mode);
	worker.run();
	return 0;
}
