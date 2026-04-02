#include "logger.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>

Logger::Logger(const std::string& filename) {
    m_file.open(filename, std::ios::app);
    if (!m_file.is_open()) {
        std::cerr << "Warning: failed to open log file: " << filename << '\n';
    }
}

Logger::~Logger() {
    if (m_file.is_open()) {
        m_file.flush();
    }
}

std::string Logger::getCurrentTimestamp() const {
    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        now.time_since_epoch()) %
                    1000;

    std::tm tm_buf{};
    std::ostringstream ss;

    if (localtime_r(&t, &tm_buf)) {
        ss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S") << '.' << std::setfill('0')
           << std::setw(3) << ms.count();
    } else {
        ss << "invalid-time";
    }
    return ss.str();
}

void Logger::log(const std::string& original, long long microseconds) {
    constexpr size_t kMaxLog = 1024;
    std::string log_str = original;
    if (log_str.size() > kMaxLog) {
        log_str.resize(kMaxLog);
        log_str += "...";
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_file.is_open()) {
        return;
    }
    m_file << '[' << getCurrentTimestamp() << "] "
           << "original: \"" << log_str << "\" | "
           << "time: " << microseconds << " us\n";
    m_file.flush();
}
