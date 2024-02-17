#pragma once

#include "world/WorldConstants.h"
#include "WorldSaveEventType.h"
#include "serialization/BitseryExt.h"

#include <boost/container/flat_map.hpp>

class World;
typedef ui32 RegionPatchIndex;

struct RegionPatchDesc {
    i32 mStartByte = 0; // Seek position
    i32 mAllocatedPages = 0;
    i32 mAllocatedBytes = 0;

    BINARY_SERIALIZE() {
        s.value4b(mStartByte);
        s.value4b(mAllocatedPages); //2b?
        s.value4b(mAllocatedBytes); //2b?
    }
};

struct RegionPendingWriteData {
    boost::container::flat_map<RegionPatchIndex, BBuffer> pendingWrites;
    ui32 incomingWrites = 0;
};

struct RegionPatchID {
    RegionID regionId;
    RegionPatchIndex regionPatchIndex;
};

struct RegionFileHeader {
    ui32 version = 0;
    std::vector<RegionPatchDesc> patches;

    ui32 getHeaderSerializeSizeBytes() const { return patches.size() * sizeof(RegionPatchDesc) + sizeof(version) + 4 /*ext size field*/; }
    BINARY_SERIALIZE() {
        s.value4b(version);
        s.ext(patches, bitsery::ext::PodStructVector{});
    }
};

enum class RegionType {
    Height,
    Biome,
    Chunk,
    COUNT
};

constexpr ui32 HEIGHT_REGION_WIDTH_PATCHES = 8;
constexpr ui32 BIOME_REGION_WIDTH_PATCHES = 8;
constexpr ui32 CHUNK_REGION_WIDTH_CHUNKS = 16;

// Each region contains multiple patches, paged so that we can update a patches data
// without rewriting the whole file. Each region is one file
class RegionFileCluster {
    friend class WorldSaveContext;
public:
    RegionFileCluster() = default;
    RegionFileCluster(ui32 worldWidthTiles, ui32 patchWidthTiles, ui32 regionWidthPatches, const char* folderName, ui32 pageSize) :
        mRegionWidthPatches(regionWidthPatches),
        mRegionWidthTiles(regionWidthPatches* patchWidthTiles),
        mWidthRegions(worldWidthTiles / mRegionWidthTiles),
        folderName(folderName),
        mPageSize(pageSize)
    {
        assert(worldWidthTiles % mRegionWidthTiles == 0);
        mRegionHeaders.resize(SQ(mWidthRegions));
        for (auto& r : mRegionHeaders) {
            r.patches.resize(SQ(regionWidthPatches));
        }
    }

    /*ui32 getRegionIndex(i32v2 worldPos) {
        return (worldPos.x / mRegionWidthTiles) + (worldPos.y / mRegionWidthTiles) * mWidthRegions;
    }*/
    size_t getRegionCount() const { return mRegionHeaders.size(); }
    ui32 getPatchesPerRegion() const { return SQ(mRegionWidthPatches); }
    ui32 getRegionWidthPatches() const { return mRegionWidthPatches; }
    ui32 getRegionWidthTiles() const { return mRegionWidthTiles; }
    ui32 getWorldWidthRegions() const { return mWidthRegions; }
    ui32 getPageSize() const { return mPageSize; }

private:
    std::vector<RegionFileHeader> mRegionHeaders;
    ui32 mRegionWidthPatches;
    ui32 mRegionWidthTiles;
    ui32 mWidthRegions;
    ui32 mPageSize;
    const char* folderName;
};

struct RegionFileClusterWriteContext {
    std::mutex mutex;
    std::map<RegionID, RegionPendingWriteData> pendingWriteData;
};

struct DeserializedRegionFileData {
    RegionFileHeader header;
    BBuffer fileBytes; // includes header

    bool isValid() const { return fileBytes.size() != 0; }

    std::span<uint8_t> getPatchBytes(ui32 patchId);
    void forEachPatch(std::function<void(ui32, std::span<uint8_t>)> func);
};

// Caches world specific save data such as regions and such
class WorldSaveContext {
public:
     WorldSaveContext(World& world);
    ~WorldSaveContext();

    void saveWorld(const fs::path& savePath);
    bool loadWorld(const fs::path& loadPath);

    EVENT_LISTENER_FUNCS(WorldSave, SaveBegin, WorldSaveEventType::SaveBegin, const WorldSaveEvent&);
    EVENT_LISTENER_FUNCS(WorldSave, SaveEnd, WorldSaveEventType::SaveEnd, const WorldSaveEvent&);

private:
    struct WorldDesc {
        ui32 worldWidth;

        BINARY_SERIALIZE() {
            s.value4b(worldWidth);
        }
    };

    void saveWorldDesc();
    WorldDesc loadWorldDesc();

    void saveHeights();
    void loadHeights();

    void saveBiomes();
    void loadBiomes();

    void saveChunks();
    void loadChunks();

    fs::path getMarkupFilePath() const;
    void saveMarkupIfNotAlreadySaved();
    void loadMarkupSynchronous();

    void loadRegionsForType(RegionType type, std::function<void(DeserializedRegionFileData&&, RegionID)> func);

    // Call after finished with all save functions
    void notifyAllDataRegistered();

    ui32 getRegionCount(RegionType type) const;

    void forEachPatchInAllRegions(RegionType type, std::function<void(ui32 patchId, RegionPatchID regionPatchId)> callback);
    void forEachPatchInRegion(RegionType type, RegionID regionId, std::function<void(ui32 patchId, RegionPatchID regionPatchId)> callback);

    // Call this when we know exactly how many onRegionPatchDataReady calls we expect
    void notifyRegionDataIncoming(RegionType type, RegionID regionId, ui32 count);
    // Call count per region should match the previously specified count
    void onRegionPatchDataReady(RegionType type, RegionPatchID patchId, BBuffer&& bbuffer);
    void dispatchRegionSaveTask(RegionType type, RegionID regionId);

    void addRegionPatchCompressAndSaveTask(RegionType type, RegionPatchID regionPatchId, BBuffer&& bbuffer);
    BBuffer compressData(const BBuffer& bbuffer);
    void decompressDataStatic(const std::span<uint8_t> compressed, uint8_t* dst, size_t dstSizeBytes);
    BBuffer decompressDataStreamed(const std::span<uint8_t> compressed, size_t reserveCount);

    DeserializedRegionFileData readRegionFile(std::fstream& file, ui32 fileSize);
    void updateRegionFile(RegionType type, RegionID regionId);
    void endSave();


    // ====================== Data ======================
    World& mWorld;
    fs::path mCurrentLoadPath;
    fs::path mCurrentSavePath;
    fs::path mPrevSavePath;

    std::array<RegionFileCluster,             e_count(RegionType)> mRegionFileClusters;
    std::array<RegionFileClusterWriteContext, e_count(RegionType)> mWriteContexts;

    std::atomic_bool mAllIncomingDataRegistered = false;
    std::atomic<ui32> mTotalSavedData = 0;
    std::atomic<ui32> mTotalIncomingData = 0;
    std::atomic_int mRunningLoadThreads = 0;
    std::mutex mEndSaveMutex;

    EVENT_DISPATCHER_DEF(WorldSave);
};

