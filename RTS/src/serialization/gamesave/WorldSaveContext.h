#pragma once

#include "filesystem/FileSystem.h"
#include "world/WorldConstants.h"

class World;

struct RegionPatchDesc {
    ui32 mStartByte = 0; // Seek position
    ui32 mCurrentSizeBytes = 0;
};

struct RegionFileHeader {
    std::vector<RegionPatchDesc> patches;
};

constexpr ui32 HEIGHT_REGION_WIDTH_PATCHES = 8;
constexpr ui32 BIOME_REGION_WIDTH_PATCHES = 8;

// Each region contains multiple patches, paged so that we can update a patches data
// without rewriting the whole file. Each region is one file
template <int PATCH_WIDTH_TILES, int REGION_WIDTH_PATCHES, int PAGE_SIZE = 4096>
class RegionFileCluster {
public:
    RegionFileCluster(ui32 worldWidthTiles) :
        mRegionWidthTiles(REGION_WIDTH_PATCHES * PATCH_WIDTH_TILES),
        mWidthRegions(worldWidthTiles / mRegionWidthTiles)
    {
        assert(worldWidthTiles % mRegionWidthTiles == 0);
        mRegionHeaders.resize(SQ(mWidthRegions));
        for (auto& r : mRegionHeaders) {
            r.patches.resize(SQ(REGION_WIDTH_PATCHES));
        }
    }

    /*ui32 getRegionIndex(i32v2 worldPos) {
        return (worldPos.x / mRegionWidthTiles) + (worldPos.y / mRegionWidthTiles) * mWidthRegions;
    }*/
    size_t getRegionCount() const { return mRegionHeaders.size(); }
    ui32 getPatchesPerRegion() const { return SQ(REGION_WIDTH_PATCHES); }

private:
    ui32 mRegionWidthTiles;
    ui32 mWidthRegions;
    std::vector<RegionFileHeader> mRegionHeaders;
};

// Caches world specific save data such as regions and such
class WorldSaveContext {
public:
     WorldSaveContext(World& world);
    ~WorldSaveContext();

private:
    World& mWorld;
    fs::path mLastSavePath;

    RegionFileCluster<HEIGHTMAP_PATCH_WIDTH_TILES, HEIGHT_REGION_WIDTH_PATCHES> mHeightHeader;
    RegionFileCluster<BIOME_PATCH_WIDTH_TILES, BIOME_REGION_WIDTH_PATCHES> mBiomeFiles;
};

