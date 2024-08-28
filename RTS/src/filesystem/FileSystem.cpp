#include "stdafx.h"
#include "FileSystem.h"

#include <windows.h>
#include <Shlobj.h>

// Mostly from https://github.com/StudioCherno/Hazel/blob/master/Hazel/src/Hazel/Utilities/FileSystem.cpp

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

bool FileSystem::createDirectory(const std::filesystem::path& directory) {
    if (fs::exists(directory) && fs::is_directory(directory))
        return true;
    return std::filesystem::create_directories(directory);
}

bool FileSystem::createDirectory(const std::string& directory) {
    if (fs::exists(directory) && fs::is_directory(directory))
        return true;
    return createDirectory(std::filesystem::path(directory));
}

bool FileSystem::createDirectories(const fs::path& directory) {
    if (fs::exists(directory) && fs::is_directory(directory))
        return true;
    return std::filesystem::create_directories(directory);
}

bool FileSystem::move(const std::filesystem::path& oldFilepath, const std::filesystem::path& newFilepath)
{
    if (FileSystem::exists(newFilepath))
        return false;
    
    try {
        std::filesystem::rename(oldFilepath, newFilepath);
    }
    catch (const std::filesystem::filesystem_error& e) {
        LOG_CRITICAL("File system move error: {}", e.what());
        return false;
    }
    return true;
}

bool FileSystem::copy(const std::filesystem::path& oldFilepath, const std::filesystem::path& newFilepath)
{
    if (FileSystem::exists(newFilepath))
        return false;

    std::filesystem::copy(oldFilepath, newFilepath);
    return true;
}

bool FileSystem::moveFile(const std::filesystem::path& filepath, const std::filesystem::path& dest)
{
    return move(filepath, dest / filepath.filename());
}

bool FileSystem::copyFile(const std::filesystem::path& filepath, const std::filesystem::path& dest)
{
    return copy(filepath, dest / filepath.filename());
}

bool FileSystem::rename(const std::filesystem::path& oldFilepath, const std::filesystem::path& newFilepath)
{
    return move(oldFilepath, newFilepath);
}

bool FileSystem::renameFilename(const std::filesystem::path& oldFilepath, const std::string& newName)
{
    // Remove any extension in new name
    std::filesystem::path newNameAsPath = newName;
    std::filesystem::path newPath = fmt::format("{0}\\{1}{2}", oldFilepath.parent_path().string(), newNameAsPath.stem().string(), oldFilepath.extension().string());
    return rename(oldFilepath, newPath);
}

bool FileSystem::exists(const std::filesystem::path& filepath)
{
    return std::filesystem::exists(filepath);
}

bool FileSystem::exists(const std::string& filepath)
{
    return std::filesystem::exists(std::filesystem::path(filepath));
}

bool FileSystem::deleteFile(const std::filesystem::path& filepath)
{
    if (!FileSystem::exists(filepath))
        return false;

    if (std::filesystem::is_directory(filepath))
        return std::filesystem::remove_all(filepath) > 0;
    return std::filesystem::remove(filepath);
}

bool FileSystem::isDirectory(const std::filesystem::path& filepath)
{
    return std::filesystem::is_directory(filepath);
}

// returns true <=> fileA was last modified more recently than fileB
bool FileSystem::isNewer(const std::filesystem::path& fileA, const std::filesystem::path& fileB)
{
    return std::filesystem::last_write_time(fileA) > std::filesystem::last_write_time(fileB);
}

bool FileSystem::showFileInExplorer(const std::filesystem::path& path)
{
    auto absolutePath = std::filesystem::canonical(path);
    if (!exists(absolutePath))
        return false;

    std::string cmd = fmt::format("explorer.exe /select,\"{0}\"", absolutePath.string());
    system(cmd.c_str());
    return true;
}

bool FileSystem::openDirectoryInExplorer(const std::filesystem::path& path)
{
    auto absolutePath = std::filesystem::canonical(path);
    if (!exists(absolutePath))
        return false;

    ShellExecuteW(NULL, L"explore", absolutePath.c_str(), NULL, NULL, SW_SHOWNORMAL);
    return true;
}

bool FileSystem::openExternally(const std::filesystem::path& path)
{
    auto absolutePath = std::filesystem::canonical(path);
    if (!exists(absolutePath))
        return false;

    ShellExecuteW(NULL, L"open", absolutePath.c_str(), NULL, NULL, SW_SHOWNORMAL);
    return true;
}

std::filesystem::path FileSystem::getUniqueFileName(const std::filesystem::path& filepath)
{
    if (!FileSystem::exists(filepath))
        return filepath;

    int counter = 0;
    auto checkID = [&counter, filepath](auto checkID) -> std::filesystem::path
    {
        ++counter;
        const std::string counterStr = [&counter] {
            if (counter < 10)
                return "0" + std::to_string(counter);
            else
                return std::to_string(counter);
        }();  // Pad with 0 if < 10;

        std::string newFileName = fmt::format("{} ({})", Utils::removeExtension(filepath.filename().string()), counterStr);

        if (filepath.has_extension())
            newFileName = fmt::format("{}{}", newFileName, filepath.extension().string());

        if (std::filesystem::exists(filepath.parent_path() / newFileName))
            return checkID(checkID);
        else
            return filepath.parent_path() / newFileName;
    };

    return checkID(checkID);
}

fs::path FileSystem::getPersistentStoragePath() {
    static std::filesystem::path s_PersistentStoragePath;

    if (!s_PersistentStoragePath.empty())
        return s_PersistentStoragePath;

    PWSTR roamingFilePath;
    HRESULT result = SHGetKnownFolderPath(FOLDERID_RoamingAppData, KF_FLAG_DEFAULT, NULL, &roamingFilePath);
    assert(result == S_OK && "Could not open roaming");
    s_PersistentStoragePath = roamingFilePath;
    s_PersistentStoragePath /= "vorb";

    if (!std::filesystem::exists(s_PersistentStoragePath))
        std::filesystem::create_directory(s_PersistentStoragePath);

    return s_PersistentStoragePath;
}
