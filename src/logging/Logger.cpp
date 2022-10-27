#include "Vorb/logging/Logger.h"

#include "spdlog/sinks/stdout_color_sinks.h"

std::shared_ptr<spdlog::logger> vorb::Logger::appLogger;
std::shared_ptr<spdlog::logger> vorb::Logger::vorbLogger;

void vorb::Logger::init(LoggingLevel level) {
    assert(!appLogger && !vorbLogger);

    spdlog::set_pattern("%^[%T] %n: %v%$");
    
    vorbLogger = spdlog::stdout_color_mt("Vorb", spdlog::color_mode::automatic);
    appLogger = spdlog::stdout_color_mt("App", spdlog::color_mode::automatic);

    setLogLevel(level);
}

void vorb::Logger::setLogLevel(LoggingLevel level) {
    vorbLogger->set_level(spdlog::level::level_enum(level));
    appLogger->set_level(spdlog::level::level_enum(level));
}

void vorb::Logger::testLogOutputs() {
    VORB_LOG_TRACE("TEST TRACE");
    VORB_LOG_DEBUG("TEST DEBUG");
    VORB_LOG_INFO("TEST INFO");
    VORB_LOG_WARN("TEST WARN");
    VORB_LOG_ERROR("TEST ERROR");
    VORB_LOG_CRITICAL("TEST CRITICAL");

    LOG_TRACE("TEST TRACE");
    LOG_DEBUG("TEST DEBUG");
    LOG_INFO("TEST INFO");
    LOG_WARN("TEST WARN");
    LOG_ERROR("TEST ERROR");
    LOG_CRITICAL("TEST CRITICAL");
}

