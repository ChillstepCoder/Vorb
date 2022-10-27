#pragma once

#include "spdlog/spdlog.h"

#define ENABLE_LOGGING 1

namespace vorb {

    enum class LoggingLevel : int {
        Trace = spdlog::level::trace,
        Debug = spdlog::level::debug,
        Info = spdlog::level::info,
        Warn = spdlog::level::warn,
        Error = spdlog::level::err,
        Critical = spdlog::level::critical,
        OFF = spdlog::level::off,
    };

    // See https://hackingcpp.com/cpp/libs/fmt.html for formatting
    class Logger
    {
    public:
        static void init(LoggingLevel level);
        static void setLogLevel(LoggingLevel level);
        // Just for testing colors and such
        static void testLogOutputs();

#if ENABLE_LOGGING == 1

        template<typename... Args>
        static void log(LoggingLevel level, const char* format, Args &&... args) {
            appLogger->log(spdlog::level::level_enum(level), format, std::forward<Args>(args)...);
        };
        template<typename... Args>
        static void logTrace(const char* format, Args &&... args) {
            appLogger->trace(format, std::forward<Args>(args)...);
        };
        template<typename... Args>
        static void logDebug(const char* format, Args &&... args) {
            appLogger->debug(format, std::forward<Args>(args)...);
        };
        template<typename... Args>
        static void logInfo(const char* format, Args &&... args) {
            appLogger->info(format, std::forward<Args>(args)...);
        };
        template<typename... Args>
        static void logWarn(const char* format, Args &&... args) {
            appLogger->warn(format, std::forward<Args>(args)...);
        };
        template<typename... Args>
        static void logError(const char* format, Args &&... args) {
            appLogger->error(format, std::forward<Args>(args)...);
        };
        template<typename... Args>
        static void logCritical(const char* format, Args &&... args) {
            appLogger->critical(format, std::forward<Args>(args)...);
        };

        template<typename... Args>
        static void vorbLog(LoggingLevel level, const char* format, Args &&... args) {
            vorbLogger->log(spdlog::level::level_enum(level), format, std::forward<Args>(args)...);
        };
        template<typename... Args>
        static void vorbLogTrace(const char* format, Args &&... args) {
            vorbLogger->trace(format, std::forward<Args>(args)...);
        };
        template<typename... Args>
        static void vorbLogDebug(const char* format, Args &&... args) {
            vorbLogger->debug(format, std::forward<Args>(args)...);
        };
        template<typename... Args>
        static void vorbLogInfo(const char* format, Args &&... args) {
            vorbLogger->info(format, std::forward<Args>(args)...);
        };
        template<typename... Args>
        static void vorbLogWarn(const char* format, Args &&... args) {
            vorbLogger->warn(format, std::forward<Args>(args)...);
        };
        template<typename... Args>
        static void vorbLogError(const char* format, Args &&... args) {
            vorbLogger->error(format, std::forward<Args>(args)...);
        };
        template<typename... Args>
        static void vorbLogCritical(const char* format, Args &&... args) {
            vorbLogger->critical(format, std::forward<Args>(args)...);
        };

#else
        static void logTrace(const char*, ...) {};
        static void logDebug(const char*, ...) {};
        static void logInfo(const char*, ...) {};
        static void logWarn(const char*, ...) {};
        static void logError(const char*, ...) {};
        static void logCritical(const char*, ...) {};
#endif
    private:
        static std::shared_ptr<spdlog::logger> appLogger;
        static std::shared_ptr<spdlog::logger> vorbLogger;

    };
}; // namespace vorb

#define LOG_MSG vorb::logger::log
#define LOG_TRACE vorb::Logger::logTrace
#define LOG_DEBUG vorb::Logger::logDebug
#define LOG_INFO vorb::Logger::logInfo
#define LOG_WARN vorb::Logger::logWarn
#define LOG_ERROR vorb::Logger::logError
#define LOG_CRITICAL vorb::Logger::logCritical

#define VORB_LOG_MSG vorb::logger::vorbLog
#define VORB_LOG_TRACE vorb::Logger::vorbLogTrace
#define VORB_LOG_DEBUG vorb::Logger::vorbLogDebug
#define VORB_LOG_INFO vorb::Logger::vorbLogInfo
#define VORB_LOG_WARN vorb::Logger::vorbLogWarn
#define VORB_LOG_ERROR vorb::Logger::vorbLogError
#define VORB_LOG_CRITICAL vorb::Logger::vorbLogCritical