#include "stdafx.h"
#include "FileSystem.h"

time_t FileSystem::getLastFileWriteTime(const Path& path) {
    // todo: 
    std::filesystem::file_time_type lastWriteTime = std::filesystem::last_write_time(path);
    return lastWriteTime.time_since_epoch().count();
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
