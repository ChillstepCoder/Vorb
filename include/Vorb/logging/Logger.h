#pragma once

#include <spdlog/spdlog.h>

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

#define LOG_DEF(CFUNC, FUNC) \
    template<typename... Args> \
    inline static void log##CFUNC(spdlog::format_string_t<Args...> format, Args &&... args) { \
        appLogger->FUNC(format, std::forward<Args>(args)...); \
    };

#define VORB_LOG_DEF(CFUNC, FUNC) \
    template<typename... Args> \
    inline static void vorbLog##CFUNC(spdlog::format_string_t<Args...> format, Args &&... args) { \
        vorbLogger->FUNC(format, std::forward<Args>(args)...); \
    };

        template<typename... Args>
        inline static void log(LoggingLevel level, spdlog::format_string_t<Args...> format, Args &&... args) {
            appLogger->log(spdlog::level::level_enum(level), format, std::forward<Args>(args)...);
        };
        LOG_DEF(Trace, trace);
        LOG_DEF(Debug, debug);
        LOG_DEF(Info, info);
        LOG_DEF(Warn, warn);
        LOG_DEF(Error, error);
        LOG_DEF(Critical, critical);

        template<typename... Args>
        inline static void vorbLog(LoggingLevel level, spdlog::format_string_t<Args...> format, Args &&... args) {
            vorbLogger->log(spdlog::level::level_enum(level), format, std::forward<Args>(args)...);
        };
        VORB_LOG_DEF(Trace, trace);
        VORB_LOG_DEF(Debug, debug);
        VORB_LOG_DEF(Info, info);
        VORB_LOG_DEF(Warn, warn);
        VORB_LOG_DEF(Error, error);
        VORB_LOG_DEF(Critical, critical);

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