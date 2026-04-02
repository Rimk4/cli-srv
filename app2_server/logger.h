/**
 * @file logger.h
 * @brief Потокобезопасная запись текстового журнала обработанных строк сервером.
 */

#pragma once

#include <fstream>
#include <mutex>
#include <string>

/**
 * @brief Журнал на базе std::ofstream с сериализацией вызовов через std::mutex.
 */
class Logger
{
public:
    /**
     * @param [in] fileName путь к файлу журнала; открывается в режиме append.
     */
    explicit Logger(const std::string& fileName);
    ~Logger();

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    /**
     * @brief Дописывает строку: локальная метка времени, исходная строка (усечённая), длительность.
     *
     * @param [in] original       полезная нагрузка запроса; при длине свыше 1024 символов усекается с суффиксом «...».
     * @param [in] microseconds   длительность обработки (здесь — вокруг чистого «reverse»), для метрик.
     */
    void log(const std::string& original, long long microseconds);

private:
    std::ofstream mFile;
    std::mutex mMutex;

    /**
     * @return локальная отметка времени YYYY-MM-DD HH:MM:SS.mmm или invalid-time при ошибке localtime_r.
     */
    std::string getCurrentTimestamp() const;
};
