#pragma once

#include <fstream>
#include <mutex>
#include <string>

class Logger {
public:
    explicit Logger(const std::string& filename);
    ~Logger();

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    void log(const std::string& original, long long microseconds);

private:
    std::ofstream m_file;
    std::mutex m_mutex;

    std::string getCurrentTimestamp() const;
};
