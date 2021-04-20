
#ifndef __LOGGER_H__
#define __LOGGER_H__

#include "spdlog/spdlog.h"
 
class XLogger
{
public:
	static XLogger* getInstance();

	// void InitXLogger(std::string logger_name, std::string log_file_name, int log_level);
	void InitXLogger(std::string log_file_name, int log_level = spdlog::level::info, bool is_console_print = false);

	/*	TRACE 0
		DEBUG 1
		INFO 2
		WARN 3
		ERROR 4
		CRITICAL 5
		OFF 6
	*/
	void setXLogLevel(int log_level);
 
	std::shared_ptr<spdlog::logger> getLogger() {
		return m_logger;
	}

	void EndXLogger();
 
private:
	// make constructor private to avoid outside instance
	XLogger();
	~XLogger();
 
	XLogger(const XLogger&) = delete;
	XLogger& operator=(const XLogger&) = delete;
 
private:
	std::shared_ptr<spdlog::logger> m_logger;
	// decide print to console or not
	bool console;
};
 
// use embedded macro to support file and line number
#define XLOG_TRACE(...) SPDLOG_LOGGER_CALL(XLogger::getInstance()->getLogger().get(), spdlog::level::trace, __VA_ARGS__)
#define XLOG_DEBUG(...) SPDLOG_LOGGER_CALL(XLogger::getInstance()->getLogger().get(), spdlog::level::debug, __VA_ARGS__)
#define XLOG_INFO(...) SPDLOG_LOGGER_CALL(XLogger::getInstance()->getLogger().get(), spdlog::level::info, __VA_ARGS__)
#define XLOG_WARN(...) SPDLOG_LOGGER_CALL(XLogger::getInstance()->getLogger().get(), spdlog::level::warn, __VA_ARGS__)
#define XLOG_ERROR(...) SPDLOG_LOGGER_CALL(XLogger::getInstance()->getLogger().get(), spdlog::level::err, __VA_ARGS__)
#define XLOG_CRITICAL(...) SPDLOG_LOGGER_CALL(XLogger::getInstance()->getLogger().get(), spdlog::level::critical, __VA_ARGS__)

#define XLOG_END()	XLogger::getInstance()->EndXLogger();
 

#endif
