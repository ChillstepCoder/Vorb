#include "stdafx.h"
#include "FileSystem.h"

time_t FileSystem::getLastFileWriteTime(const Path& path) {
    try {
        auto tp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(fs::last_write_time(path) - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
        std::time_t fileTimeT = std::chrono::system_clock::to_time_t(tp);
        return fileTimeT;
    }
    catch (const fs::filesystem_error& e) {
        LOG_CRITICAL("File system getLastFileWriteTime error: {}", e.what());
    }
    catch (const std::exception& e) {
        LOG_CRITICAL("File system getLastFileWriteTime exception: {}", e.what());
    }
    return 0;
}

std::string FileSystem::fileTimeToString(time_t time) {
    std::tm local_time;
    if (errno_t errorCode = localtime_s(&local_time, &time)) {
        LOG_CRITICAL("Failed to parse file time {} to local time - error: {}", time, errorCode);
        return "INVALID TIME";
    }
    std::stringstream ss;
    ss << std::put_time(&local_time, "%Y-%m-%d %H:%M:%S");
    return ss.str();
}
