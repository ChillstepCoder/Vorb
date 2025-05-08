#include "stdafx.h"
#include "GameSaveManager.h"

//https://github.com/facebook/zstd
#include <zstd.h>

#include "world/World.h"
#include "world/host/HostWorldData.h"
#include "serialization/gamesave/WorldSaveContext.h"

#include "services/Services.h"

struct WorldTemplateHeaderData {
    ui32 version = 0;
    ui32 seed = 0;

    template <typename S>
    void serialize(S& s) {
        s.value4b(version);
        s.value4b(seed);
    }
};

// Selected based on average use case
// over 2 gigs
constexpr ui32 START_SIZE = 2200000000;

GameSaveManager::GameSaveManager() {
    mThread = std::make_unique<std::thread>(&GameSaveManager::saveThreadFunc, this);
}

GameSaveManager::~GameSaveManager() {
    mQuitThread = true;
    mDiskIOTasks.enqueue([]() {});
    while (mIsSavingWorld) Sleep(4);
    mThread->join();
}

GameSaveManager& GameSaveManager::get() {
    static GameSaveManager sInstance;
    return sInstance;
}

bool GameSaveManager::saveWorld(World& world, const nString& fileName, bool blockUntilFinished) {
    if (mIsSavingWorld) {
        return false;
    }
    PreciseTimer timer2;
    LOG_INFO("Saving world {}", fileName.c_str());
    mIsSavingWorld = true;
    mCurrentWorldSaveContext = &world.getSaveContext();

    mCurrentWorldSaveContext->registerWorldSaveListeners(mSaveEventListeners);
    mCurrentWorldSaveContext->addSaveEndListener(mSaveEventListeners, [this](const WorldSaveEvent& e) {
        mSaveEventListeners.reset();
        mIsSavingWorld = false;
    });

    mCurrentWorldSaveContext->saveWorld(getSavesDirectory() / fileName);
    LOG_INFO("World serialize took {} ms", timer2.stop());

    if (blockUntilFinished) {
        while (mIsSavingWorld) {
            Sleep(16);
        }
    }
}

bool GameSaveManager::loadWorld(World& outWorld, const fs::path& savePath) {
    if (mIsSavingWorld) {
        return false;
    }
    return outWorld.getSaveContext().loadWorld(savePath);
}

void GameSaveManager::notifyWorldSaveFinished() {
    assert(mIsSavingWorld);
    mIsSavingWorld = false;
    // TODO: Dispatch event?
}

fs::path GameSaveManager::getSavesDirectory() {
    fs::path currentPath = fs::current_path();
    return currentPath / "saves" / "world";
}

void GameSaveManager::saveThreadFunc() {
    setThreadName("Disk IO");

    constexpr size_t BULK_DEQUEUE_COUNT = 64;
    std::function<void()> func;
    while (!mQuitThread.load()) {
        mDiskIOTasks.wait_dequeue(func);
        func();
    }
}
