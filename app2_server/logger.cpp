/**
 * @file logger.cpp
 * @brief Реализация Logger: временные метки через localtime_r (reentrant), flush на каждую запись.
 */

#include "logger.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>

Logger::Logger(const std::string& fileName)
{
    mFile.open(fileName, std::ios::app);
    if (!mFile.is_open())
    {
        std::cerr << "Warning: failed to open log file: " << fileName << '\n';
    }
}

Logger::~Logger()
{
    if (mFile.is_open())
    {
        mFile.flush();
    }
}

std::string Logger::getCurrentTimestamp() const
{
    const auto now = std::chrono::system_clock::now();
    const std::time_t timeT = std::chrono::system_clock::to_time_t(now);
    const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(
                            now.time_since_epoch()) %
                        1000;

    std::tm tmBuf{};
    std::ostringstream outStream;

    if (localtime_r(&timeT, &tmBuf))
    {
        outStream << std::put_time(&tmBuf, "%Y-%m-%d %H:%M:%S") << '.' << std::setfill('0')
           << std::setw(3) << millis.count();
    }
    else
    {
        outStream << "invalid-time";
    }

    return outStream.str();
}

void Logger::log(const std::string& original, long long microseconds)
{
    constexpr size_t maxLogChars = 1024;
    std::string logStr = original;
    if (logStr.size() > maxLogChars)
    {
        logStr.resize(maxLogChars);
        logStr += "...";
    }

    std::lock_guard<std::mutex> lock(mMutex);
    if (!mFile.is_open())
    {
        return;
    }
    mFile << '[' << getCurrentTimestamp() << "] "
           << "original: \"" << logStr << "\" | "
           << "time: " << microseconds << " us\n";
    mFile.flush();
}
