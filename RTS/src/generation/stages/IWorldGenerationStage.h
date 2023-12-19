#pragma once

class WorldDataGenerator;
class IHeightmapGrid;
class HostWorldData;
class BiomeGrid;
class WorldGenerationBlackboard;
struct WorldGenerationData;

class IWorldGenerationStage
{
public:
    IWorldGenerationStage(WorldDataGenerator& generator);
    virtual ~IWorldGenerationStage() = default;

    virtual void begin() = 0;

    virtual const char* getStageName() const = 0;

    // Return true on complete
    virtual bool update() = 0;

    virtual void abort();

protected:
    virtual void abortInternal() {};
    WorldDataGenerator& mGenerator;
    WorldGenerationData& mGenerationData;
    WorldGenerationBlackboard& mBlackboard;

    HostWorldData* mWorldData = nullptr;
    IHeightmapGrid* mHeightGrid = nullptr;
    BiomeGrid* mBiomeGrid = nullptr;

    VGBuffer mHeightSSBO = 0;
    VGTexture mHeightTexture = 0;
    VGBuffer mBiomeSSBO = 0;
    VGTexture mBiomeTexture = 0;
    GLfloat* mMappedHeights = nullptr;
    ui32* mMappedBiomes = nullptr;

    f32 mWorldSeed = 0.f;
    ui32 mWorldSeedInt = 0;
    ui32 mTotalHeightPatches = 0;

    // Shared by all stages, represents one whole generation
    inline static std::atomic<ui32> sGenerationUID = 0;
};

