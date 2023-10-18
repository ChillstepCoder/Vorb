#pragma once

#include <functional>
#include <filesystem>

namespace fs = std::filesystem;

enum class FileSystemAction : ui8{
    Added, Rename, Modified, Delete
};

struct FileSystemChangedEvent {
    FileSystemAction Action;
    fs::path FilePath;
    bool IsDirectory;

    // If this is a rename event the new name will be in the FilePath
    std::wstring OldName = L"";
};

// Mostly from https://github.com/StudioCherno/Hazel/blob/master/Hazel/src/Hazel/Utilities/FileSystem.h
class FileSystem
{
public:
    typedef fs::path Path;

    // Return system clock units of last file write time
    static time_t getLastFileWriteTime(const Path& path);
    static std::string fileTimeToString(time_t time);

    static bool createDirectory(const fs::path& directory);
    static bool createDirectory(const std::string& directory);
    static bool exists(const fs::path& filepath);
    static bool exists(const std::string& filepath);
    static bool deleteFile(const fs::path& filepath);
    static bool moveFile(const fs::path& filepath, const fs::path& dest);
    static bool copyFile(const fs::path& filepath, const fs::path& dest);
    static bool isDirectory(const fs::path& filepath);

    static bool isNewer(const fs::path& fileA, const fs::path& fileB);

    static bool move(const fs::path& oldFilepath, const fs::path& newFilepath);
    static bool copy(const fs::path& oldFilepath, const fs::path& newFilepath);
    static bool rename(const fs::path& oldFilepath, const fs::path& newFilepath);
    static bool renameFilename(const fs::path& oldFilepath, const std::string& newName);

    static bool showFileInExplorer(const fs::path& path);
    static bool openDirectoryInExplorer(const fs::path& path);
    static bool openExternally(const fs::path& path);

    static fs::path getUniqueFileName(const fs::path& filepath);

//public:
//
//    static void startWatching();
//    static void stopWatching();
//
//    static fs::path openFileDialog(const char* filter = "All\0*.*\0");
//    static fs::path openFolderDialog(const char* initialFolder = "");
//    static fs::path saveFileDialog(const char* filter = "All\0*.*\0");
//
//    static fs::path getPersistentStoragePath();
//
//    static void skipNextFileSystemChange();
//
//public:
//    static bool hasEnvironmentVariable(const std::string& key);
//    static bool setEnvironmentVariable(const std::string& key, const std::string& value);
//    static std::string getEnvironmentVariable(const std::string& key);
//
//private:
//    static unsigned long watch(void* param);

};
