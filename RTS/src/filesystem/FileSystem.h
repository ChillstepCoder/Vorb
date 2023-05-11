#pragma once

#include <filesystem>

class FileSystem
{
public:
    typedef std::filesystem::path Path;

    // Return system clock units of last file write time
    static time_t getLastFileWriteTime(const Path& path);
    static std::string fileTimeToString(time_t time);
};

