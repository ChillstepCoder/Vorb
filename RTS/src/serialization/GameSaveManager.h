#pragma once

class World;
class HostWorldData;
class WorldSaveContext;

#include "filesystem/FileSystem.h"
#include "serialization/gamesave/WorldSaveEventType.h"

class GameSaveManager
{
public:
    GameSaveManager();
    ~GameSaveManager();

    static GameSaveManager& get();

    // Save on generation screen for later use
    // Returns false if we are already saving
    bool saveWorld(World& world, const nString& fileName, bool blockUntilFinished);
    bool loadWorld(World& outWorld, const fs::path& savePath);
    bool saveWorldTemplate(World& world);
    bool loadWorldTemplate(HostWorldData& worldData);
    //void loadWorldTemplate(World& world);

    void addDiskIOTask(std::function<void()> func) { mDiskIOTasks.enqueue(func); }

    void notifyWorldSaveFinished();
    fs::path getSavesDirectory();
private:
    fs::path getTemplatesDirectory();
    void saveThreadFunc();

    void saveHeightData(World& world);
    void saveWorldTemplateV0(World& world);
    void serializeWorldTemplateData(World& world, BBuffer& templateData, ui32 version);
    void compressAndWriteFile(const fs::path& filename, const BBuffer& data);

    moodycamel::BlockingConcurrentQueue<std::function<void()>> mDiskIOTasks;
    std::atomic_bool mIsSavingWorld = false;
    std::atomic_bool mQuitThread = false;
    std::unique_ptr<std::thread> mThread;
    WorldSaveContext* mCurrentWorldSaveContext = nullptr;

    WorldSaveListeners mSaveEventListeners;

};

