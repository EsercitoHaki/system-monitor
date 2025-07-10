#include "system_data.h"
#include <fstream>
#include <sstream>
#include <dirent.h>
#include <algorithm>
#include <iostream>
#include <string>
#include <limits>
#include <sys/statvfs.h> 
#include <cstring> 
#include <cerrno> 

SystemData::SystemData() {
    initializeSensors();
    prev_cpu_stats_ = readCpuStats();
    last_cpu_update_time_ = std::chrono::steady_clock::now();
}

void SystemData::initializeSensors() {
    sensors_.clear();
    findHwmonSensors();
    // TODO: Add more sophisticated logic for CPU/GPU detection if needed
}

double SystemData::readTemperatureFromFile(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Error opening file: " << filepath << std::endl;
        return -1.0;
    }
    long value_milli_celsius;
    file >> value_milli_celsius;
    file.close();
    return static_cast<double>(value_milli_celsius) / 1000.0;
}

double SystemData::getCpuTemperature() {
    for (const auto& sensor : sensors_) {
        if (sensor.name.find("Core") != std::string::npos ||
            sensor.name.find("Package") != std::string::npos ||
            sensor.name.find("cpu_thermal") != std::string::npos) {
            return readTemperatureFromFile(sensor.path);
        }
    }
    return -1.0;
}

double SystemData::getGpuTemperature() {
    for (const auto& sensor : sensors_) {
        if (sensor.name.find("GPU") != std::string::npos ||
            sensor.name.find("Nvidia") != std::string::npos ||
            sensor.name.find("AMD") != std::string::npos ||
            sensor.name.find("radeon") != std::string::npos ||
            sensor.name.find("amdgpu") != std::string::npos) {
            return readTemperatureFromFile(sensor.path);
        }
    }
    return -1.0;
}

double SystemData::getTemperature(const std::string& sensor_name) {
    for (const auto& sensor : sensors_) {
        if (sensor.name == sensor_name) {
            return readTemperatureFromFile(sensor.path);
        }
    }
    return -1.0;
}

std::map<std::string, double> SystemData::getAllTemperatures() {
    std::map<std::string, double> all_temps;
    for (const auto& sensor : sensors_) {
        all_temps[sensor.name] = readTemperatureFromFile(sensor.path);
    }
    return all_temps;
}

void SystemData::findHwmonSensors() {
    std::string hwmon_path = "/sys/class/hwmon/";
    DIR* dir = opendir(hwmon_path.c_str());
    if (!dir) {
        std::cerr << "Could not open " << hwmon_path << std::endl;
        return;
    }

    dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        std::string hwmon_dir_name = entry->d_name;
        if (hwmon_dir_name == "." || hwmon_dir_name == "..") continue;

        std::string full_hwmon_path = hwmon_path + hwmon_dir_name + "/";

        std::ifstream name_file(full_hwmon_path + "name");
        std::string chip_name;
        if (name_file.is_open()) {
            std::getline(name_file, chip_name);
            name_file.close();
        } else {
            chip_name = "Unknown_" + hwmon_dir_name;
        }

        DIR* sensor_dir = opendir(full_hwmon_path.c_str());
        if (!sensor_dir) continue;

        dirent* sensor_entry;
        while ((sensor_entry = readdir(sensor_dir)) != NULL) {
            std::string filename = sensor_entry->d_name;
            if (filename.rfind("temp", 0) == 0 && filename.rfind("_input") == filename.length() - 6) {
                SensorInfo info;
                info.path = full_hwmon_path + filename;

                std::ifstream label_file(full_hwmon_path + filename.substr(0, filename.length() - 6) + "_label");
                if (label_file.is_open()) {
                    std::getline(label_file, info.name);
                    label_file.close();
                } else {
                    info.name = chip_name + " " + filename.substr(0, filename.length() - 6);
                }
                sensors_.push_back(info);
            }
        }
        closedir(sensor_dir);
    }
    closedir(dir);
}

CpuStats SystemData::readCpuStats() {
    CpuStats current_stats = {0};
    std::ifstream file("/proc/stat");
    if (!file.is_open()) {
        std::cerr << "Error opening /proc/stat" << std::endl;
        return current_stats;
    }

    std::string line;
    std::getline(file, line);
    file.close();

    std::stringstream ss(line);
    std::string cpu_label;
    ss >> cpu_label >> current_stats.user >> current_stats.nice >> current_stats.system >> current_stats.idle
       >> current_stats.iowait >> current_stats.irq >> current_stats.softirq >> current_stats.steal
       >> current_stats.guest >> current_stats.guest_nice;

    current_stats.total = current_stats.user + current_stats.nice + current_stats.system + current_stats.idle +
                          current_stats.iowait + current_stats.irq + current_stats.softirq + current_stats.steal +
                          current_stats.guest + current_stats.guest_nice;
    return current_stats;
}

double SystemData::getCpuUsage() {
    CpuStats current_stats = readCpuStats();
    std::chrono::steady_clock::time_point current_time = std::chrono::steady_clock::now();

    auto elapsed_seconds = std::chrono::duration_cast<std::chrono::duration<double>>(current_time - last_cpu_update_time_).count();

    double cpu_usage = 0.0;
    if (elapsed_seconds > 0.01) {
        long idle_delta = (current_stats.idle + current_stats.iowait) - (prev_cpu_stats_.idle + prev_cpu_stats_.iowait);
        long total_delta = current_stats.total - prev_cpu_stats_.total;

        if (total_delta > 0) {
            cpu_usage = (static_cast<double>(total_delta - idle_delta) / total_delta) * 100.0;
        }
    }
    cpu_usage_history_.push_back(cpu_usage);
    if (cpu_usage_history_.size() > MAX_HISTORY_POINTS) {
        cpu_usage_history_.pop_front();
    }

    prev_cpu_stats_ = current_stats;
    last_cpu_update_time_ = current_time;

    return cpu_usage;
}

long SystemData::parseMemInfoLine(const std::string& line, const std::string& key) {
    if (line.rfind(key, 0) == 0) {
        std::stringstream ss(line);
        std::string label;
        long value;
        std::string unit;
        ss >> label >> value >> unit;
        return value;
    }
    return -1;
}

MemoryInfo SystemData::getMemoryInfo() {
    MemoryInfo mem_info = {0, 0, 0, 0, 0.0};
    std::ifstream file("/proc/meminfo");
    if (!file.is_open()) {
        std::cerr << "Error opening /proc/meminfo" << std::endl;
        return mem_info;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.rfind("MemTotal:", 0) == 0) {
            mem_info.total_kb = parseMemInfoLine(line, "MemTotal:");
        } else if (line.rfind("MemFree:", 0) == 0) {
            mem_info.free_kb = parseMemInfoLine(line, "MemFree:");
        } else if (line.rfind("MemAvailable:", 0) == 0) {
            mem_info.available_kb = parseMemInfoLine(line, "MemAvailable:");
        }
    }
    file.close();

    if (mem_info.available_kb > 0) {
        mem_info.used_kb = mem_info.total_kb - mem_info.available_kb;
    } else {
        mem_info.used_kb = mem_info.total_kb - mem_info.free_kb;
    }

    if (mem_info.total_kb > 0) {
        mem_info.usage_percent = (static_cast<double>(mem_info.used_kb) / mem_info.total_kb) * 100.0;
    }
    return mem_info;
}

DiskInfo SystemData::getDiskUsage(const std::string& path) {
    DiskInfo disk_info = {path, 0, 0, 0, 0.0};
    struct statvfs vfs;

    if (statvfs(path.c_str(), &vfs) == 0) {
        unsigned long long total_blocks = vfs.f_blocks;
        unsigned long long free_blocks = vfs.f_bavail;
        unsigned long long block_size = vfs.f_bsize;

        disk_info.total_space_gb = (total_blocks / (1024ULL * 1024ULL * 1024ULL / block_size)) * (block_size / (1024ULL * 1024ULL * 1024ULL / block_size));
        if (block_size < 1024ULL * 1024ULL * 1024ULL) {
             disk_info.total_space_gb = (total_blocks * block_size) / (1024 * 1024 * 1024);
        } else {
            disk_info.total_space_gb = total_blocks * (block_size / (1024 * 1024 * 1024));
        }

        if (disk_info.total_space_gb == 0 && total_blocks > 0) {
             disk_info.total_space_gb = 1;
        }

        disk_info.free_space_gb = (free_blocks * block_size) / (1024 * 1024 * 1024);
        disk_info.used_space_gb = disk_info.total_space_gb - disk_info.free_space_gb;

        if (disk_info.total_space_gb > 0) {
            disk_info.usage_percent = (static_cast<double>(disk_info.used_space_gb) / disk_info.total_space_gb) * 100.0;
        } else {
            disk_info.usage_percent = 0.0;
        }

    } else {
        std::cerr << "Error getting disk stats for " << path << ": " << strerror(errno) << std::endl;
        disk_info.total_space_gb = -1;
        disk_info.used_space_gb = -1;
        disk_info.free_space_gb = -1;
        disk_info.usage_percent = -1.0;
    }
    return disk_info;
}