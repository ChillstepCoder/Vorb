#include "stdafx.h"
#include "GameSaveManager.h"

//https://github.com/facebook/zstd
#include <zstd.h>

#include "world/World.h"
#include "world/host/HostWorldData.h"

GameSaveManager::GameSaveManager() {
    mThread = std::make_unique<std::thread>(&GameSaveManager::saveThreadFunc, this);
}

GameSaveManager::~GameSaveManager() {
    mQuitThread = true;
    mSaveFuncs.enqueue([]() {});
    mThread->join();
}

GameSaveManager& GameSaveManager::get() {
    static GameSaveManager sInstance;
    return sInstance;
}

bool GameSaveManager::saveWorldTemplate(World& world) {
    if (mIsSavingWorld) {
        return false;
    }
    mIsSavingWorld = true;
    saveWorldTemplateV0(world);
}

struct WorldTemplateHeaderData {
    ui32 version = 0;

    template <typename S>
    void serialize(S& s) {
        s.value4b(version);
    }
};


void GameSaveManager::saveThreadFunc() {
    SIM_THREAD_ID = std::this_thread::get_id();
    setThreadName("Sim");

    constexpr size_t BULK_DEQUEUE_COUNT = 64;
    std::function<void()> func;
    while (!mQuitThread.load()) {
        mSaveFuncs.wait_dequeue(func);
        func();
    }
}

// TODO: Unit Tests?
void GameSaveManager::saveWorldTemplateV0(World& world) {
    PreciseTimer timer;

    BBuffer templateData;
    serializeWorldTemplateData(world, templateData, 0);

    LOG_INFO("  Finished in {} ms - Sending to save thread", timer.elapsedMs());
    mSaveFuncs.enqueue([this, &world, templateData = std::move(templateData)]() {
        fs::path currentPath = fs::current_path();
        fs::path templatesFolder = currentPath / "saves" / "templates";
        if (!fs::exists(templatesFolder)) {
            if (!fs::create_directories(templatesFolder)) {
                panic("Failed to create {} directory. Insufficient permissions?", templatesFolder.string());
            }
        }

        PreciseTimer timer;

        LOG_INFO("  Saving world template to {}", templatesFolder.string().c_str());
        compressAndWriteFile(templatesFolder / "template.dat", templateData);
        LOG_INFO("  Finished in {} ms", timer.elapsedMs());

        mIsSavingWorld = false;
    });
}


void GameSaveManager::serializeWorldTemplateData(World& world, BBuffer& templateData, ui32 version) {
    LOG_INFO("Binary packing world template data...");
    IHeightmapGrid& heightGrid = world.getHeightmapGrid();
    BiomeGrid& biomeGrid = world.getBiomeGrid();
    WorldMarkupGrid& markupGrid = world.getMarkupGrid();
    assert(heightGrid.mHeightData);

    // Selected based on average use case
    // A bit over a gigabyte
    constexpr ui32 START_SIZE = 1400000000;
    templateData.reserve(START_SIZE);


    LOG_INFO("AAA {}", templateData.capacity());

    // TODO: In production use bitsery::ext::Growable{} for forward/backwards compatability
    https://github.com/fraillt/bitsery/blob/master/examples/forward_backward_compatibility.cpp

    WorldTemplateHeaderData header;
    header.version = version;
    bitsery::Serializer<BOutputAdapter> s{ BOutputAdapter{ templateData } };
    // Serialize
    s.object(header);
    LOG_INFO("  Serializing heights...");
    PreciseTimer timer2;
    s.object(heightGrid);
    LOG_CRITICAL("  HEIGHT FINISH in {} ms", timer2.elapsedMs());
    LOG_INFO("  Serializing biomes...");
    timer2.start();
    s.object(biomeGrid);
    LOG_CRITICAL("  BIOME FINISH in {} ms", timer2.elapsedMs());
    LOG_INFO("  Serializing markup...");
    timer2.start();
    s.object(markupGrid);
    LOG_CRITICAL("  MARKUP FINISH in {} ms", timer2.elapsedMs());

    s.adapter().flush();

    templateData.resize(s.adapter().writtenBytesCount());

    if (templateData.capacity() > START_SIZE) {
        LOG_CRITICAL("World template save performance warning! Intial buffer size of {} exceeded to {}", START_SIZE, templateData.capacity());
        __debugbreak();
    }
}

void GameSaveManager::compressAndWriteFile(const fs::path& filename, const BBuffer& data) {
    BBuffer compressed(data.size());

    LOG_INFO("  Compressing...");
    // TODO: Dictionary compression for better speed and ratio
    size_t const cSize = ZSTD_compress(compressed.data(), compressed.size(), data.data(), data.size(), 1);
    if (ZSTD_isError(cSize)) {
        panic("ZSTD_compress failed during template save for {}", filename.string().c_str());
    }

    LOG_INFO("  Saving to disk...");
    std::ofstream file(filename, std::ios::binary);
    file.write(reinterpret_cast<char*>(compressed.data()), cSize);
}