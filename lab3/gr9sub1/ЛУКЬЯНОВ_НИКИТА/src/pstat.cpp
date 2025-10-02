#include <cstdint>
#include <iostream>
#include <fstream>
#include <vector>
#include <sstream>
#include <iomanip>
#include <unordered_map>
#include <unistd.h>
#include "json.hpp"

using json = nlohmann::json;

class ProcFileReader {
public:
    explicit ProcFileReader() {

    }

    ~ProcFileReader() = default;

    bool readKeyValue(const std::string& path, std::unordered_map<std::string, std::string>& table) {
        mFileStream.open(path);

        if (!mFileStream.is_open()) {
            std::cout << "Failed to open " + path << std::endl;
            return false;
        }

        std::string infoLine;
        while (std::getline(mFileStream, infoLine)) {
            std::stringstream ss(infoLine);
            std::string name, info;
            ss >> name >> info;

            if (!name.empty() && name.back() == ':')
                name.pop_back();

            if (table.find(name) != table.end())
                table[name] = info;
        }

        mFileStream.close();

        return true;
    }

    bool readStatFile(const std::string& path, std::unordered_map<std::string, std::string>& table) {
        mFileStream.open(path);
        if (!mFileStream.is_open())
            return false;

        std::vector<std::string> fields;
        std::string info;

        while (mFileStream >> info)
            fields.push_back(info);

        if (fields.size() >= 15) {
            table["utime"] = fields[13];
            table["stime"] = fields[14];
        }

        mFileStream.close();

        return true;
    }

private:
    std::ifstream mFileStream;
};

class ProcStat {
public:
    explicit ProcStat(const uint32_t pid) : mPid(pid) {

    }

    std::vector<std::pair<std::string, std::string>> getStatistics() {
        std::unordered_map<std::string, std::string> table;
        const long hz = sysconf(_SC_CLK_TCK);
        if (!readConfigFile(table)) {
            return {};
        }
        ProcFileReader procFileReader;
        std::string strPid = std::to_string(mPid);

        const std::string procPath = "/proc/" + strPid;

        if (!procFileReader.readKeyValue(procPath + "/status", table) ||
            !procFileReader.readKeyValue(procPath + "/io", table) ||
            !procFileReader.readStatFile(procPath + "/stat", table)) {
            return {};
        }

        std::vector<std::pair<std::string, std::string>> result;

        for (const auto& field : mConfig["fields"]) {
            std::string info = "[not found]";
            
            if(table.find(field["name"]) != table.end())
                info = table[field["name"]];

            result.emplace_back(field["name"], formatValue(info, field["unit"], hz));
        }

        return result;
    }

    [[nodiscard]]
    uint32_t getPid() const {
        return mPid;
    }

    void setPid(const uint32_t pid) {
        mPid = pid;
    }

private:
    bool readConfigFile(std::unordered_map<std::string, std::string>& table) {
        table.clear();

        std::ifstream configFile("config.json");
        if (!configFile.is_open()) {
            std::cerr << "Failed to open config.json" << std::endl;
            return false;
        }

        configFile >> mConfig;

        for (const auto& field : mConfig["fields"]) {
            table[field["name"]] = "";
        }

        return true;
    }

    static std::string formatValue(const std::string& raw, const std::string& unit, const int hz) {
        if (unit == "raw") return raw;

        const double val = std::stod(raw);
        std::ostringstream oss;
        if (unit == "KiB")
            oss << std::fixed << std::setprecision(2) << val << " KiB";
        else if(unit == "MiB")
            oss << std::fixed << std::setprecision(2) << val / (1024.0) << " MiB";
        else if (unit == "ticks")
            oss << val << " ticks (" << val / hz << " sec)";
        else
            oss << raw;

        return oss.str();
    }

    uint32_t mPid{};
    json mConfig;
};

int main(int argc, char* argv[]) {

    if (argc != 2) {
        std::cout << "Incorrect number of parameters!" << std::endl;
        return 1;
    }

    uint32_t pid{};

    try {
        pid = std::stoi(argv[1]);
    }
    catch(const std::exception& e) {
        std::cout << "Pid is not a number!" << std::endl;
    }

    if(pid <= 0 || pid > UINT32_MAX) {
        std::cout << "Pid is out of range!" << std::endl;
        return 2;
    }

    ProcStat proc_stat(pid);

    for (const auto & data: proc_stat.getStatistics()) {
        std::cout << data.first << " " << data.second << std::endl;
    }

    return 0;
}