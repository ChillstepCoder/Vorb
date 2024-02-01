#pragma once

class World;

#include "filesystem/FileSystem.h"

class GameSaveManager
{
public:
    GameSaveManager();
    ~GameSaveManager();

    static GameSaveManager& get();

    // Save on generation screen for later use
    // Returns false if we are already saving
    bool saveWorldTemplate(World& world);
    //void loadWorldTemplate(World& world);

private:
    void saveThreadFunc();

    void saveWorldTemplateV0(World& world);
    void serializeWorldTemplateData(World& world, BBuffer& templateData, ui32 version);
    void compressAndWriteFile(const fs::path& filename, const BBuffer& data);

    moodycamel::BlockingConcurrentQueue<std::function<void()>> mSaveFuncs;
    std::atomic_bool mIsSavingWorld = false;
    std::atomic_bool mQuitThread = false;
    std::unique_ptr<std::thread> mThread;

};

