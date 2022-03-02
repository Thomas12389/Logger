
#include "spdlog/async.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/rotating_file_sink.h"

#include "Logger.h"
 
XLogger* XLogger::getInstance() {
	static XLogger xlogger;
	return &xlogger;
}

XLogger::XLogger() {
	
}
 
XLogger::~XLogger() {
	EndXLogger();
}

void XLogger::EndXLogger() {
	spdlog::drop_all(); // must do this
	spdlog::shutdown();
}

void XLogger::InitXLogger(std::string log_dir, std::string log_file_name, int log_level, bool is_console_print) {
	if (is_console_print) {
		console = true;
	}
	
	// log path
	const std::string log_path = log_dir + log_file_name;
	// 输出重定向到文件, for debug
	// freopen(log_path.c_str(), "a+", stdout);

	try {
		m_logger = spdlog::create_async<spdlog::sinks::rotating_file_sink_mt>(log_file_name, log_path, 5 * 1024 * 1024, 3); // multi part log files, with every part 5M, max 3 files

		// flush immediately when an error level above encountered
		m_logger->flush_on(spdlog::level::err);

		// Flush all *registered* loggers using a worker thread every 3 seconds.
		// note: registered loggers *must* be thread safe for this to work correctly!
		// spdlog::flush_every(std::chrono::seconds(3));
		spdlog::flush_every(std::chrono::seconds(1));

#if defined(TRACE_TRACES)
		m_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e %z] <thread %t> [%=9l] [%@] %v"); // with timestamp, thread_id, filename and line number
		m_logger->set_level(spdlog::level::trace);
#elif defined(DEBUG_TRACES)
		// custom format
		m_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e %z] <thread %t> [%=9l] [%@] %v"); // with timestamp, thread_id, filename and line number
		m_logger->set_level(spdlog::level::debug);
#else
		m_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e %z] [%=9l] %v");
		m_logger->set_level(static_cast<spdlog::level::level_enum>(log_level));
#endif

	}
	catch (const spdlog::spdlog_ex &ex) {
		printf("Log initialization failed: %s\n", ex.what());
	}
}

void XLogger::setXLogLevel(int log_level) {
	m_logger->set_level(static_cast<spdlog::level::level_enum>(log_level));
}
