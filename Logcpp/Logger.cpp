#include "Logger.h"
#include <ctime>
#include <sstream>
#include <algorithm>
#include <Windows.h>  // Windows API for file operations and console colors

// Windows.h defines macros that conflict with our LogLevel enum values
// e.g., wingdi.h: #define ERROR 0
// Undefine them so our enum works correctly
#ifdef ERROR
#undef ERROR
#endif
#ifdef DEBUG
#undef DEBUG
#endif
#ifdef WARNING
#undef WARNING
#endif

// Get console color attribute for log level (Windows Console API)
// Must be in .cpp since it depends on Windows.h
static WORD logLevelToColorAttribute(LogLevel level) {
	switch (level) {
	case LogLevel::DEBUG:   return 7;    // White (default)
	case LogLevel::INFO:    return 10;   // Green
	case LogLevel::WARNING: return 14;   // Yellow
	case LogLevel::ERROR:   return 12;   // Red
	case LogLevel::FATAL:   return 13;   // Magenta
	default:                return 7;    // Default white
	}
}

// Get singleton instance (Meyers' Singleton)
Logger& Logger::getInstance() {
	static Logger instance;  // C++11 guarantees thread-safe static local initialization
	return instance;
}

Logger::Logger()
	: m_minLevel(LogLevel::DEBUG)
	, m_consoleOutput(true)
	, m_maxFileSize(10 * 1024 * 1024)
	, m_maxBackupFiles(5)
	, m_initialized(false)
	, m_running(false)
	, m_totalLogs(0)
	, m_errorLogs(0)
	, m_warningLogs(0)
	, m_consoleHandle(nullptr) {
	// Get console handle for colored output
	m_consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
}

Logger::~Logger() {
	shutdown();
}

bool Logger::init(const std::string& filename, LogLevel minLevel,
	bool consoleOutput, size_t maxFileSize, size_t maxBackupFiles) {
	std::lock_guard<std::mutex> lock(m_mutex);

	if (m_initialized) {
		std::cerr << "Logger already initialized!" << std::endl;
		return false;
	}

	m_filename = filename;
	m_minLevel = minLevel;
	m_consoleOutput = consoleOutput;
	m_maxFileSize = maxFileSize;
	m_maxBackupFiles = maxBackupFiles;

	// Open log file
	m_fileStream.open(m_filename, std::ios::app);
	if (!m_fileStream.is_open()) {
		std::cerr << "Failed to open log file: " << m_filename << std::endl;
		return false;
	}

	// Start async log worker thread
	m_running = true;
	m_workerThread = std::unique_ptr<std::thread>(new std::thread(&Logger::processLogQueue, this));

	m_initialized = true;

	// Write startup log
	std::string startMsg = "========================================\n";
	startMsg += "Logger initialized successfully\n";
	startMsg += "Log file: " + m_filename + "\n";
	startMsg += "Min level: " + logLevelToString(m_minLevel) + "\n";
	startMsg += "Console output: " + std::string(m_consoleOutput ? "enabled" : "disabled") + "\n";
	startMsg += "Max file size: " + std::to_string(m_maxFileSize) + " bytes\n";
	startMsg += "Max backup files: " + std::to_string(m_maxBackupFiles) + "\n";
	startMsg += "========================================\n";

	writeLog(startMsg, LogLevel::INFO);

	return true;
}

void Logger::shutdown() {
	if (!m_initialized) return;

	// Stop worker thread
	m_running = false;
	m_queueCV.notify_all();

	if (m_workerThread && m_workerThread->joinable()) {
		m_workerThread->join();
	}

	// Flush buffer
	flushBuffer();

	// Write shutdown log
	std::string shutdownMsg = "========================================\n";
	shutdownMsg += "Logger shutting down\n";
	shutdownMsg += "Total logs: " + std::to_string(m_totalLogs.load()) + "\n";
	shutdownMsg += "Errors: " + std::to_string(m_errorLogs.load()) + "\n";
	shutdownMsg += "Warnings: " + std::to_string(m_warningLogs.load()) + "\n";
	shutdownMsg += "========================================\n";

	if (m_fileStream.is_open()) {
		m_fileStream << shutdownMsg;
		m_fileStream.flush();
		m_fileStream.close();
	}

	m_initialized = false;
}

void Logger::setLogLevel(LogLevel level) {
	std::lock_guard<std::mutex> lock(m_mutex);
	m_minLevel = level;

	std::string msg = "Log level changed to: " + logLevelToString(level);
	writeLog(msg, LogLevel::INFO);
}

void Logger::enableConsoleOutput(bool enable) {
	std::lock_guard<std::mutex> lock(m_mutex);
	m_consoleOutput = enable;
}

void Logger::log(LogLevel level, const std::string& message,
	const char* file, int line, const char* function) {
	if (!m_initialized) return;
	if (level < m_minLevel) return;

	// Update statistics
	m_totalLogs++;
	if (level == LogLevel::ERROR || level == LogLevel::FATAL) {
		m_errorLogs++;
	}
	else if (level == LogLevel::WARNING) {
		m_warningLogs++;
	}

	// Format message
	std::string formattedMessage = formatMessage(level, message, file, line, function);

	// Add to queue
	{
		std::lock_guard<std::mutex> lock(m_queueMutex);
		m_logQueue.push(std::make_pair(formattedMessage, level));
	}
	m_queueCV.notify_one();
}

void Logger::debug(const std::string& message, const char* file, int line, const char* function) {
	log(LogLevel::DEBUG, message, file, line, function);
}

void Logger::info(const std::string& message, const char* file, int line, const char* function) {
	log(LogLevel::INFO, message, file, line, function);
}

void Logger::warning(const std::string& message, const char* file, int line, const char* function) {
	log(LogLevel::WARNING, message, file, line, function);
}

void Logger::error(const std::string& message, const char* file, int line, const char* function) {
	log(LogLevel::ERROR, message, file, line, function);
}

void Logger::fatal(const std::string& message, const char* file, int line, const char* function) {
	log(LogLevel::FATAL, message, file, line, function);
}

std::string Logger::formatMessage(LogLevel level, const std::string& message,
	const char* file, int line, const char* function) {
	std::ostringstream oss;

	// Timestamp
	oss << "[" << getCurrentTimestamp() << "] ";

	// Log level
	oss << "[" << logLevelToString(level) << "] ";

	// Thread ID
	oss << "[Thread-" << getThreadId() << "] ";

	// File, line number, function (if available)
	if (file && function) {
		// Extract filename only (not full path)
		const char* filename = strrchr(file, '\\');
		if (!filename) filename = strrchr(file, '/');
		filename = filename ? filename + 1 : file;

		oss << "[" << filename << ":" << line << " " << function << "] ";
	}

	// Log message
	oss << message;

	return oss.str();
}

void Logger::writeConsoleColored(const std::string& text, LogLevel level) {
	if (m_consoleHandle == nullptr || m_consoleHandle == INVALID_HANDLE_VALUE) {
		// Fallback: plain text output
		if (level >= LogLevel::ERROR) {
			std::cerr << text << std::endl;
		}
		else {
			std::cout << text << std::endl;
		}
		return;
	}

	HANDLE hConsole = static_cast<HANDLE>(m_consoleHandle);
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	WORD defaultColor = 7; // Default white on black

						   // Save current console color
	if (GetConsoleScreenBufferInfo(hConsole, &csbi)) {
		defaultColor = csbi.wAttributes;
	}

	// Set color based on log level
	WORD color = logLevelToColorAttribute(level);
	SetConsoleTextAttribute(hConsole, color);

	// Write to appropriate stream
	if (level >= LogLevel::ERROR) {
		std::cerr << text << std::endl;
	}
	else {
		std::cout << text << std::endl;
	}

	// Restore original color
	SetConsoleTextAttribute(hConsole, defaultColor);
}

void Logger::writeLog(const std::string& formattedMessage, LogLevel level) {
	// Output to console with color
	if (m_consoleOutput) {
		writeConsoleColored(formattedMessage, level);
	}

	// Write to file (thread-safe)
	{
		std::lock_guard<std::mutex> lock(m_fileMutex);

		if (m_fileStream.is_open()) {
			m_fileStream << formattedMessage << std::endl;

			// Periodic flush: every 100 logs or on ERROR/FATAL
			// flushCounter is now protected by m_fileMutex (no data race)
			static int flushCounter = 0;
			if (++flushCounter >= 100 || level >= LogLevel::ERROR) {
				m_fileStream.flush();
				flushCounter = 0;

				// Check file size for rotation
				checkAndRotateFile();
			}
		}
	}
}

void Logger::checkAndRotateFile() {
	if (!m_fileStream.is_open()) return;

	// Flush before checking size
	m_fileStream.flush();

	// Use Windows API to get file size
	HANDLE hFile = CreateFileA(m_filename.c_str(), GENERIC_READ,
		FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL, NULL);

	if (hFile == INVALID_HANDLE_VALUE) return;

	LARGE_INTEGER fileSize;
	if (!GetFileSizeEx(hFile, &fileSize)) {
		CloseHandle(hFile);
		return;
	}
	CloseHandle(hFile);

	if (static_cast<size_t>(fileSize.QuadPart) >= m_maxFileSize) {
		// Close current file
		m_fileStream.close();

		// Rotate backup files
		for (size_t i = m_maxBackupFiles; i > 0; --i) {
			std::string oldName;
			std::string newName = m_filename + "." + std::to_string(i);

			if (i == 1) {
				oldName = m_filename;
			}
			else {
				oldName = m_filename + "." + std::to_string(i - 1);
			}

			// Delete oldest backup
			if (i == m_maxBackupFiles) {
				DeleteFileA(newName.c_str());
			}

			// Rename
			if (GetFileAttributesA(oldName.c_str()) != INVALID_FILE_ATTRIBUTES) {
				MoveFileA(oldName.c_str(), newName.c_str());
			}
		}

		// Reopen new file
		m_fileStream.open(m_filename, std::ios::trunc);
		if (m_fileStream.is_open()) {
			m_fileStream << "[Logger] Log file rotated at "
				<< getCurrentTimestamp() << std::endl;
		}
	}
}

std::string Logger::getCurrentTimestamp() const {
	auto now = std::chrono::system_clock::now();
	auto now_c = std::chrono::system_clock::to_time_t(now);
	auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
		now.time_since_epoch()) % 1000;

	struct tm timeinfo;
	localtime_s(&timeinfo, &now_c);  // VS2015 safe version

	std::ostringstream oss;
	oss << std::put_time(&timeinfo, "%Y-%m-%d %H:%M:%S");
	oss << "." << std::setfill('0') << std::setw(3) << ms.count();

	return oss.str();
}

std::string Logger::getThreadId() const {
	std::ostringstream oss;
	oss << std::this_thread::get_id();
	return oss.str();
}

void Logger::processLogQueue() {
	while (m_running) {
		std::pair<std::string, LogLevel> logEntry;

		{
			std::unique_lock<std::mutex> lock(m_queueMutex);
			m_queueCV.wait(lock, [this] {
				return !m_logQueue.empty() || !m_running;
			});

			if (!m_running && m_logQueue.empty()) {
				break;
			}

			logEntry = m_logQueue.front();
			m_logQueue.pop();
		}

		writeLog(logEntry.first, logEntry.second);
	}

	// Drain remaining logs
	while (!m_logQueue.empty()) {
		auto& logEntry = m_logQueue.front();
		writeLog(logEntry.first, logEntry.second);
		m_logQueue.pop();
	}
}

void Logger::flushBuffer() {
	std::lock_guard<std::mutex> lock(m_fileMutex);
	if (m_fileStream.is_open()) {
		m_fileStream.flush();
	}
}