#ifndef LOGGER_H
#define LOGGER_H

#include <iostream>
#include <fstream>
#include <string>
#include <mutex>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <thread>
#include <queue>
#include <condition_variable>
#include <atomic>
#include <memory>
#include <ctime>
#include <cstdio>

// Log level enum
enum class LogLevel {
    DEBUG = 0,
    INFO = 1,
    WARNING = 2,
    ERROR = 3,
    FATAL = 4
};

// Convert log level to string
inline std::string logLevelToString(LogLevel level) {
    switch (level) {
    case LogLevel::DEBUG:   return "DEBUG";
    case LogLevel::INFO:    return "INFO";
    case LogLevel::WARNING: return "WARNING";
    case LogLevel::ERROR:   return "ERROR";
    case LogLevel::FATAL:   return "FATAL";
    default:                return "UNKNOWN";
    }
}

// ANSI color codes (kept for non-Windows or modern terminal compatibility)
inline std::string logLevelToColor(LogLevel level) {
    switch (level) {
    case LogLevel::DEBUG:   return "\033[37m";  // White
    case LogLevel::INFO:    return "\033[32m";  // Green
    case LogLevel::WARNING: return "\033[33m";  // Yellow
    case LogLevel::ERROR:   return "\033[31m";  // Red
    case LogLevel::FATAL:   return "\033[35m";  // Magenta
    default:                return "\033[0m";   // Default
    }
}

class Logger {
public:
    // Get singleton instance (Meyers' Singleton - C++11 thread-safe)
    static Logger& getInstance();

    // Initialize the logger system
    bool init(const std::string& filename = "app.log",
        LogLevel minLevel = LogLevel::DEBUG,
        bool consoleOutput = true,
        size_t maxFileSize = 10 * 1024 * 1024,  // 10MB
        size_t maxBackupFiles = 5);

    // Shutdown the logger system
    void shutdown();

    // Set minimum log level
    void setLogLevel(LogLevel level);

    // Enable/disable console output
    void enableConsoleOutput(bool enable);

    // Log a message
    void log(LogLevel level, const std::string& message,
        const char* file = nullptr, int line = 0,
        const char* function = nullptr);

    // Convenience methods
    void debug(const std::string& message, const char* file = nullptr,
        int line = 0, const char* function = nullptr);
    void info(const std::string& message, const char* file = nullptr,
        int line = 0, const char* function = nullptr);
    void warning(const std::string& message, const char* file = nullptr,
        int line = 0, const char* function = nullptr);
    void error(const std::string& message, const char* file = nullptr,
        int line = 0, const char* function = nullptr);
    void fatal(const std::string& message, const char* file = nullptr,
        int line = 0, const char* function = nullptr);

    // Disable copy and assignment
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    // Destructor
    ~Logger();

private:
    Logger();

    // Format a log message
    std::string formatMessage(LogLevel level, const std::string& message,
        const char* file, int line, const char* function);

    // Write log to console and file
    void writeLog(const std::string& formattedMessage, LogLevel level);

    // Check and rotate log file if needed
    void checkAndRotateFile();

    // Get current timestamp as string
    std::string getCurrentTimestamp() const;

    // Get current thread ID as string
    std::string getThreadId() const;

    // Background worker thread function
    void processLogQueue();

    // Flush file buffer
    void flushBuffer();

    // Write colored text to Windows console
    void writeConsoleColored(const std::string& text, LogLevel level);

private:
    // Logger configuration
    std::string m_filename;
    LogLevel m_minLevel;
    bool m_consoleOutput;
    size_t m_maxFileSize;
    size_t m_maxBackupFiles;
    bool m_initialized;

    // File stream
    std::ofstream m_fileStream;

    // Thread safety
    std::mutex m_mutex;
    std::mutex m_fileMutex;

    // Async log queue
    std::queue<std::pair<std::string, LogLevel>> m_logQueue;
    std::condition_variable m_queueCV;
    std::mutex m_queueMutex;
    std::unique_ptr<std::thread> m_workerThread;
    std::atomic<bool> m_running;

    // Statistics (atomic for lock-free reads)
    std::atomic<size_t> m_totalLogs;
    std::atomic<size_t> m_errorLogs;
    std::atomic<size_t> m_warningLogs;

    // Console handle for colored output (Windows)
    void* m_consoleHandle;
};

// Convenience macros for logging
#define LOG_DEBUG(msg)   Logger::getInstance().debug(msg, __FILE__, __LINE__, __FUNCTION__)
#define LOG_INFO(msg)    Logger::getInstance().info(msg, __FILE__, __LINE__, __FUNCTION__)
#define LOG_WARNING(msg) Logger::getInstance().warning(msg, __FILE__, __LINE__, __FUNCTION__)
#define LOG_ERROR(msg)   Logger::getInstance().error(msg, __FILE__, __LINE__, __FUNCTION__)
#define LOG_FATAL(msg)   Logger::getInstance().fatal(msg, __FILE__, __LINE__, __FUNCTION__)

// Formatted log macros (VS2015 compatible, uses _snprintf_s)
// Use do-while(0) idiom for safe macro expansion in all contexts
// __VA_ARGS__ with ## is a GCC/MSVC extension for optional args (supported by VS2015)

#define LOG_DEBUG_F(format, ...) \
    do { \
        char _log_buf[4096]; \
        _snprintf_s(_log_buf, sizeof(_log_buf), _TRUNCATE, format, ##__VA_ARGS__); \
        Logger::getInstance().debug(_log_buf, __FILE__, __LINE__, __FUNCTION__); \
    } while(0)

#define LOG_INFO_F(format, ...) \
    do { \
        char _log_buf[4096]; \
        _snprintf_s(_log_buf, sizeof(_log_buf), _TRUNCATE, format, ##__VA_ARGS__); \
        Logger::getInstance().info(_log_buf, __FILE__, __LINE__, __FUNCTION__); \
    } while(0)

#define LOG_WARNING_F(format, ...) \
    do { \
        char _log_buf[4096]; \
        _snprintf_s(_log_buf, sizeof(_log_buf), _TRUNCATE, format, ##__VA_ARGS__); \
        Logger::getInstance().warning(_log_buf, __FILE__, __LINE__, __FUNCTION__); \
    } while(0)

#define LOG_ERROR_F(format, ...) \
    do { \
        char _log_buf[4096]; \
        _snprintf_s(_log_buf, sizeof(_log_buf), _TRUNCATE, format, ##__VA_ARGS__); \
        Logger::getInstance().error(_log_buf, __FILE__, __LINE__, __FUNCTION__); \
    } while(0)

#define LOG_FATAL_F(format, ...) \
    do { \
        char _log_buf[4096]; \
        _snprintf_s(_log_buf, sizeof(_log_buf), _TRUNCATE, format, ##__VA_ARGS__); \
        Logger::getInstance().fatal(_log_buf, __FILE__, __LINE__, __FUNCTION__); \
    } while(0)

#endif // LOGGER_H