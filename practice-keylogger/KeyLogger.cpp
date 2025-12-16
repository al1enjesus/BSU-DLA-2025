#include <ApplicationServices/ApplicationServices.h>
#include <Carbon/Carbon.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <mutex>
#include <iomanip>
#include <sstream>
#include <csignal>
#include <ctime>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>

class SimpleKeylogger {
private:
    std::atomic<bool> isRunning;
    std::ofstream logFile;
    struct termios oldTermios;

    void disableBuffering() {
        tcgetattr(STDIN_FILENO, &oldTermios);
        struct termios newTermios = oldTermios;
        newTermios.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &newTermios);
    }

    void restoreBuffering() {
        tcsetattr(STDIN_FILENO, TCSANOW, &oldTermios);
    }

public:
    SimpleKeylogger() : isRunning(false) {
        std::time_t t = std::time(nullptr);
        std::tm* tm = std::localtime(&t);
        std::ostringstream oss;
        oss << "simple_log_" << std::put_time(tm, "%Y%m%d_%H%M%S") << ".txt";
        logFile.open(oss.str());

        if (!logFile.is_open()) {
            std::cerr << "Failed to open log file\n";
        }
    }

    ~SimpleKeylogger() {
        stop();
        if (logFile.is_open()) {
            logFile.close();
        }
        restoreBuffering();
    }

    bool start() {
        disableBuffering();
        isRunning = true;

        std::cout << "Simple Keylogger started (Terminal input only)\n";
        std::cout << "Press 'q' to quit\n";
        std::cout << "Note: Only captures input in THIS terminal window\n\n";

        return true;
    }

    void stop() {
        isRunning = false;
        restoreBuffering();
    }

    void run() {
        char c;
        auto startTime = std::chrono::system_clock::now();

        while (isRunning) {
            if (read(STDIN_FILENO, &c, 1) > 0) {
                if (c == 'q' || c == 'Q') {
                    stop();
                    break;
                }

                auto now = std::chrono::system_clock::now();
                auto duration = now - startTime;
                auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration);

                // Log the key
                std::string logEntry = "[" + std::to_string(ms.count()) + "ms] '" + c + "' (ASCII: " +
                    std::to_string(static_cast<int>(c)) + ")";

                std::cout << logEntry << std::endl;

                if (logFile.is_open()) {
                    logFile << logEntry << std::endl;
                    logFile.flush();
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
};

int main() {
    std::cout << "=======================================\n";
    std::cout << "SIMPLE KEYLOGGER (Terminal Demo)\n";
    std::cout << "=======================================\n";
    std::cout << "WARNING: For educational purposes only!\n";
    std::cout << "This version only logs input in THIS terminal.\n";
    std::cout << "=======================================\n\n";

    SimpleKeylogger logger;

    if (!logger.start()) {
        return 1;
    }

    signal(SIGINT, [](int) {
        std::cout << "\nExiting...\n";
        exit(0);
    });

    logger.run();

    std::cout << "\nProgram finished.\n";
    return 0;
}