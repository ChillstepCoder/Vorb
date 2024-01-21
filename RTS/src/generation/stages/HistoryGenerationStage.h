#pragma once
#include "IWorldGenerationStage.h"
#include "world/biome/BiomeCorruptions.h"
#include "world/World.h"

struct SimThreadEntityRequest;
class HostSimContext;

constexpr ui64 HISTORY_GEN_DURATION_REAL_TIME_HOURS = 12;

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

struct WorldMarkupData {
    
};

// Allocates the world and generates history
class HistoryGenerationStage : public IWorldGenerationStage
{
public:
    HistoryGenerationStage(WorldDataGenerator& generator, std::unique_ptr<World>& worldPtr);
    ~HistoryGenerationStage();

    void begin() override;

    const char* getStageName() const override { return "History"; }

    bool update() override;

    f32 getProgress() const override { return glm::min(mHistoryProgress, 1.0f); }

    void renderImguiControls() override;
    void debugDraw() override;

private:
    void allocateWorld();
    void generateWorldMarkup();

    // History events
    void handleHistoryEvent(HistoryEvent& event);
    void handleCorruptSpawn(BiomeCorruptions type);
    
    void updateBiomes();
    void growBiomesStep();
    void downloadBiomes();

    f32 mHistoryProgress = 0.0f; // [0,1]
    TimestampMs mHistoryDurationMS = HISTORY_GEN_DURATION_REAL_TIME_HOURS * 60 * 60 * MS_PER_SECOND;
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

    bool mDrawCharacters = true;

    std::shared_ptr<SimThreadEntityRequest> mPrevCharacterRequest;
    std::shared_ptr<SimThreadEntityRequest> mCurrentCharacterRequest;

    std::unique_ptr<World>& mWorldPtr;
    HostSimContext* mHostSimContext = nullptr;
};

