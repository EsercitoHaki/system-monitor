#ifndef SYSTEM_DATA_H
#define SYSTEM_DATA_H

#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <deque>

struct SensorInfo {
    std::string name;
    std::string path;
};

struct CpuStats {
    long user;
    long nice;
    long system;
    long idle;
    long iowait;
    long irq;
    long softirq;
    long steal;
    long guest;
    long guest_nice;
    long total;
};

struct MemoryInfo {
    long total_kb;
    long free_kb;
    long available_kb;
    long used_kb;
    double usage_percent;
};

struct DiskInfo {
    std::string mount_point;
    long total_space_gb;
    long used_space_gb;
    long free_space_gb;
    double usage_percent;
};

class SystemData {
public:
    SystemData();

    double getCpuTemperature();
    double getGpuTemperature();
    double getTemperature(const std::string& sensor_name);
    std::map<std::string, double> getAllTemperatures();

    double getCpuUsage();

    const std::deque<double>& getCpuUsageHistory() const { return cpu_usage_history_; }

    MemoryInfo getMemoryInfo();

    DiskInfo getDiskUsage(const std::string& path);

    size_t getMaxHistoryPoints() const { return MAX_HISTORY_POINTS; }

private:
    std::vector<SensorInfo> sensors_;
    void initializeSensors();
    double readTemperatureFromFile(const std::string& filepath);
    void findHwmonSensors();

    CpuStats prev_cpu_stats_;
    std::chrono::steady_clock::time_point last_cpu_update_time_;
    std::deque<double> cpu_usage_history_;
    const size_t MAX_HISTORY_POINTS = 60;

    CpuStats readCpuStats();
    long parseMemInfoLine(const std::string& line, const std::string& key);
};

#endif