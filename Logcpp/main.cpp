#include "Logger.h"
#include <vector>
#include <chrono>
#include <random>
#include <iomanip>
#include <string>

// Performance test function
void performanceTest() {
	const int NUM_LOGS = 100000;
	std::vector<std::string> testMessages = {
		"User login successful",
		"Database query executed",
		"Cache miss for key: user_123",
		"Network request timeout",
		"Memory allocation failed",
		"Configuration file loaded",
		"SSL certificate verified",
		"API rate limit exceeded",
		"File upload completed",
		"Background task finished"
	};

	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<> msgDist(0, static_cast<int>(testMessages.size()) - 1);
	std::uniform_int_distribution<> levelDist(0, 4);

	auto start = std::chrono::high_resolution_clock::now();

	for (int i = 0; i < NUM_LOGS; ++i) {
		LogLevel level = static_cast<LogLevel>(levelDist(gen));
		std::string msg = testMessages[msgDist(gen)] + " #" + std::to_string(i);

		switch (level) {
		case LogLevel::DEBUG:
			LOG_DEBUG(msg);
			break;
		case LogLevel::INFO:
			LOG_INFO(msg);
			break;
		case LogLevel::WARNING:
			LOG_WARNING(msg);
			break;
		case LogLevel::ERROR:
			LOG_ERROR(msg);
			break;
		case LogLevel::FATAL:
			LOG_FATAL(msg);
			break;
		}
	}

	auto end = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

	std::cout << "\n========================================" << std::endl;
	std::cout << "Performance Test Results:" << std::endl;
	std::cout << "Total logs: " << NUM_LOGS << std::endl;
	std::cout << "Time taken: " << duration.count() << " ms" << std::endl;
	if (duration.count() > 0) {
		std::cout << "Logs per second: " << (NUM_LOGS * 1000.0 / duration.count()) << std::endl;
	}
	else {
		std::cout << "Logs per second: (too fast to measure)" << std::endl;
	}
	std::cout << "========================================\n" << std::endl;
}

// Multi-thread test
void multiThreadTest() {
	const int NUM_THREADS = 10;
	const int LOGS_PER_THREAD = 1000;

	std::vector<std::thread> threads;

	auto start = std::chrono::high_resolution_clock::now();

	for (int t = 0; t < NUM_THREADS; ++t) {
		threads.emplace_back([t, LOGS_PER_THREAD]() {
			for (int i = 0; i < LOGS_PER_THREAD; ++i) {
				LOG_INFO_F("Thread %d: Processing item %d/%d", t, i + 1, LOGS_PER_THREAD);

				if (i % 100 == 0) {
					std::this_thread::sleep_for(std::chrono::milliseconds(1));
				}
			}
		});
	}

	for (auto& thread : threads) {
		thread.join();
	}

	auto end = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

	std::cout << "\n========================================" << std::endl;
	std::cout << "Multi-thread Test Results:" << std::endl;
	std::cout << "Threads: " << NUM_THREADS << std::endl;
	std::cout << "Logs per thread: " << LOGS_PER_THREAD << std::endl;
	std::cout << "Total logs: " << (NUM_THREADS * LOGS_PER_THREAD) << std::endl;
	std::cout << "Time taken: " << duration.count() << " ms" << std::endl;
	std::cout << "========================================\n" << std::endl;
}

// Exception handling test
void exceptionTest() {
	try {
		throw std::runtime_error("Test exception occurred");
	}
	catch (const std::exception& e) {
		LOG_ERROR_F("Caught exception: %s", e.what());

		try {
			// Simulate nested exception
			throw std::runtime_error("Nested exception while handling error");
		}
		catch (const std::exception& nested) {
			LOG_FATAL_F("Fatal nested exception: %s", nested.what());
		}
	}
}

int main() {
	std::cout << "========================================" << std::endl;
	std::cout << "Logger System Test Program" << std::endl;
	std::cout << "========================================\n" << std::endl;

	// 1. Initialize logger system
	Logger& logger = Logger::getInstance();
	if (!logger.init("test.log", LogLevel::DEBUG, false, 1024 * 1024, 3)) {
		std::cerr << "Failed to initialize logger!" << std::endl;
		return -1;
	}

	std::cout << "Logger initialized successfully!\n" << std::endl;

	// 2. Basic functionality test
	std::cout << "=== Basic Functionality Test ===" << std::endl;

	LOG_DEBUG("This is a debug message");
	LOG_INFO("This is an info message");
	LOG_WARNING("This is a warning message");
	LOG_ERROR("This is an error message");
	LOG_FATAL("This is a fatal message");

	// 3. Formatted message test
	std::cout << "\n=== Formatted Message Test ===" << std::endl;

	LOG_INFO_F("User %s logged in from IP %s", "admin", "192.168.1.100");
	LOG_WARNING_F("Disk usage at %d%%", 85);
	LOG_ERROR_F("Failed to connect to database at %s:%d", "localhost", 5432);

	// 4. Log level test
	std::cout << "\n=== Log Level Test ===" << std::endl;

	logger.setLogLevel(LogLevel::WARNING);
	LOG_DEBUG("This debug message should NOT appear");
	LOG_INFO("This info message should NOT appear");
	LOG_WARNING("This warning message SHOULD appear");
	LOG_ERROR("This error message SHOULD appear");

	logger.setLogLevel(LogLevel::DEBUG);

	// 5. Exception handling test
	std::cout << "\n=== Exception Handling Test ===" << std::endl;
	exceptionTest();

	// 6. Performance test
	std::cout << "\n=== Performance Test ===" << std::endl;
	performanceTest();

	// 7. Multi-thread test
	std::cout << "\n=== Multi-thread Test ===" << std::endl;
	multiThreadTest();

	// 8. Console output toggle test
	std::cout << "\n=== Console Output Toggle Test ===" << std::endl;
	logger.enableConsoleOutput(false);
	LOG_INFO("This message goes only to file");
	logger.enableConsoleOutput(false);
	LOG_INFO("This message goes to both console and file");

	// 9. Shutdown logger
	std::cout << "\n=== Shutting Down Logger ===" << std::endl;
	logger.shutdown();

	std::cout << "\n========================================" << std::endl;
	std::cout << "All tests completed successfully!" << std::endl;
	std::cout << "Check 'test.log' for the complete log output." << std::endl;
	std::cout << "========================================" << std::endl;

	return 0;
}