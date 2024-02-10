#pragma once

class World;
class HostWorldData;

#include "filesystem/FileSystem.h"

class GameSaveManager
{
public:
    GameSaveManager();
    ~GameSaveManager();

    static GameSaveManager& get();

    // Save on generation screen for later use
    // Returns false if we are already saving
    bool saveWorld(World& world, const nString& fileName);
    bool saveWorldTemplate(World& world);
    bool loadWorldTemplate(HostWorldData& worldData);
    //void loadWorldTemplate(World& world);

private:
    fs::path getTemplatesDirectory();
    void saveThreadFunc();

    void saveHeightData(World& world);
    void saveWorldTemplateV0(World& world);
    void serializeWorldTemplateData(World& world, BBuffer& templateData, ui32 version);
    void compressAndWriteFile(const fs::path& filename, const BBuffer& data);

    moodycamel::BlockingConcurrentQueue<std::function<void()>> mSaveFuncs;
    std::atomic_bool mIsSavingWorld = false;
    std::atomic_int mRunningSaveThreads = 0;
    std::atomic_bool mQuitThread = false;
    std::unique_ptr<std::thread> mThread;

};

