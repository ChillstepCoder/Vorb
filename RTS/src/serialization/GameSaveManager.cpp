#include "stdafx.h"
#include "GameSaveManager.h"

//https://github.com/facebook/zstd
#include <zstd.h>

#include "world/World.h"
#include "world/host/HostWorldData.h"

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
    mSaveFuncs.enqueue([]() {});
    mThread->join();
}

GameSaveManager& GameSaveManager::get() {
    static GameSaveManager sInstance;
    return sInstance;
}

bool GameSaveManager::saveWorld(World& world, const nString& fileName)
{
    if (mIsSavingWorld) {
        return false;
    }
    mIsSavingWorld = true;
}

bool GameSaveManager::saveWorldTemplate(World& world) {
    if (mIsSavingWorld) {
        return false;
    }
    mIsSavingWorld = true;
    saveWorldTemplateV0(world);
}

bool GameSaveManager::loadWorldTemplate(HostWorldData& worldData) {

    const fs::path templatesFolder = getTemplatesDirectory();
    const fs::path filePath = templatesFolder / "template.dat";

    if (!fs::exists(filePath)) {
        panic("No template found at {}", filePath.string().c_str());
        return false;
    }

    LOG_INFO("Loading world template from {}", filePath.string().c_str());
    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        panic("Failed to open template file {}", filePath.string().c_str());
        return false;
    }
    PreciseTimer timer, timer3;
    LOG_INFO("Loading template...");
    size_t uncompressedSize;
    const size_t compressedSize = (size_t)file.tellg() - sizeof(size_t);
    BBuffer compressedData;
    compressedData.resize(compressedSize);
    file.seekg(0);
    // TODO: Does not support endianness
    file.read(reinterpret_cast<char*>(&uncompressedSize), sizeof(size_t));
    file.read(reinterpret_cast<char*>(compressedData.data()), compressedSize);
    file.close();
    LOG_INFO("   Read in {} ms", timer.elapsedMs());
    timer.start();

    BBuffer decompressed(uncompressedSize);
    const size_t dSize = ZSTD_decompress(decompressed.data(), decompressed.size(), compressedData.data(), compressedData.size());
    if (ZSTD_isError(dSize)) {
        panic("ZSTD_decompress failed during template load for {}", filePath.string().c_str());
    }
    decompressed.resize(dSize);

    LOG_INFO("   Decompress in {} ms", timer.elapsedMs());
    timer.start();
    WorldTemplateHeaderData header;
    bitsery::Deserializer<BInputAdapter> d{ BInputAdapter{ decompressed.begin(), decompressed.end() } };
    d.object(header);
    LOG_INFO("  Deserializing heights...");
    PreciseTimer timer2;
    assert(worldData.heightmapGrid);
    d.object(*worldData.heightmapGrid);
    LOG_CRITICAL("  HEIGHT FINISH in {} ms", timer2.elapsedMs());
    LOG_INFO("  Deserializing biomes...");
    timer2.start();
    assert(worldData.biomeGrid);
    d.object(*worldData.biomeGrid);
    LOG_CRITICAL("  BIOME FINISH in {} ms", timer2.elapsedMs());
    LOG_INFO("  Deserializing markup...");
    timer2.start();
    assert(worldData.markupGrid);
    d.object(*worldData.markupGrid);
    LOG_CRITICAL("  MARKUP FINISH in {} ms", timer2.elapsedMs());
    LOG_INFO("  Deserializing tiles...");
    timer2.start();
    assert(worldData.tileGrid);
    d.object(*worldData.tileGrid);
    LOG_CRITICAL("  TILES FINISH in {} ms", timer2.elapsedMs());
    LOG_INFO("  Finished in {} ms", timer3.elapsedMs());

    return true;
}

fs::path GameSaveManager::getTemplatesDirectory() {
    fs::path currentPath = fs::current_path();
    return currentPath / "saves" / "templates";
}

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

void GameSaveManager::saveHeightData(World& world) {

}

// TODO: Unit Tests?
void GameSaveManager::saveWorldTemplateV0(World& world) {
    PreciseTimer timer;

    BBuffer templateData;
    serializeWorldTemplateData(world, templateData, 0);

    LOG_INFO("  Finished in {} ms - Sending to save thread", timer.elapsedMs());
    mSaveFuncs.enqueue([this, &world, templateData = std::move(templateData)]() {
        const fs::path templatesFolder = getTemplatesDirectory();
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
    SimChunkTileGrid& tileGrid = world.getSimTileGrid();
    assert(heightGrid.mHeightData);

    templateData.reserve(START_SIZE);

    // TODO: In production use bitsery::ext::Growable{} for forward/backwards compatability
    https://github.com/fraillt/bitsery/blob/master/examples/forward_backward_compatibility.cpp

    WorldTemplateHeaderData header;
    header.version = version;
    header.seed = world.getSeed();
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
    LOG_INFO("  Serializing tiles...");
    timer2.start();
    s.object(tileGrid);
    LOG_CRITICAL("  TILES FINISH in {} ms", timer2.elapsedMs());

    s.adapter().flush();

    templateData.resize(s.adapter().writtenBytesCount());

    if (templateData.capacity() > START_SIZE) {
        LOG_CRITICAL("World template save performance warning! Intial buffer size of {} exceeded to {}", START_SIZE, templateData.capacity());
        __debugbreak();
    }
}

void GameSaveManager::compressAndWriteFile(const fs::path& filename, const BBuffer& data) {
    BBuffer compressed(data.size());

    LOG_INFO("  Compressing... {}", data.size() / 1024. / 1024.);
    // TODO: Dictionary compression for better speed and ratio
    size_t const cSize = ZSTD_compress(compressed.data(), compressed.size(), data.data(), data.size(), 1);
    if (ZSTD_isError(cSize)) {
        panic("ZSTD_compress failed during template save for {}", filename.string().c_str());
    }

    LOG_INFO("  Saving {} to disk...", cSize / 1024. / 1024.);
    std::ofstream file(filename, std::ios::binary);
    size_t uncompressedSize = data.size();
    // TODO: Does not support endianness
    file.write(reinterpret_cast<char*>(&uncompressedSize), sizeof(size_t));
    file.write(reinterpret_cast<char*>(compressed.data()), cSize);
}