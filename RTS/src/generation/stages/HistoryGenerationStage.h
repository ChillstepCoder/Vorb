#pragma once
#include "IWorldGenerationStage.h"
#include "world/biome/BiomeCorruptions.h"
#include "world/World.h"

class HostSimContext;

constexpr ui64 HISTORY_GEN_DURATION_HOURS = 2;

enum class HistoryEventType {
    ChernobogSpawn,
    BanshiraSpawn,
    COUNT
};

struct HistoryEvent {
    HistoryEventType type;
    f32 time;
};

struct BiomeGrowPass {
    ~BiomeGrowPass();
    GLsync sync = 0;
    bool isOdd;
    int row;
    ui32 numBlocks;
};

// Allocates the world and generates history
class HistoryGenerationStage : public IWorldGenerationStage
{
public:
    HistoryGenerationStage(WorldDataGenerator& generator, std::unique_ptr<World>& worldPtr) :
        IWorldGenerationStage(generator), mWorldPtr(worldPtr) {}

    void begin() override;

    const char* getStageName() const override { return "History"; }

    bool update() override;

    f32 getProgress() const override { return glm::min(mHistoryProgress, 1.0f); }

private:
    void allocateWorld();
    // History events
    void handleHistoryEvent(HistoryEvent& event);
    void handleCorruptSpawn(BiomeCorruptions type);
    
    void updateBiomes();
    void growBiomesStep();
    void downloadBiomes();

    f32 mHistoryProgress = 0.0f; // [0,1]
    TimestampMs mHistoryDurationMS =  HISTORY_GEN_DURATION_HOURS * 60 * 60 * MS_PER_SECOND;
    ui32 mTickCount = 0;
    ui32 mNextEventIndex = 0;
    // Sorted by time
    std::vector<HistoryEvent> mHistoryEvents;
    bool allEventsTriggered = false;
    BiomeGrowPass mCurBiomeGrowPass;
    bool mGrowPassIsOdd = false;
    int mGrowPassCount = 0;
    int mGrowPassRowBlockIndex = 0;
    int mGrowPassRowsPerPass = 0;

    std::unique_ptr<World>& mWorldPtr;
    HostSimContext* mHostSimContext = nullptr;
};

