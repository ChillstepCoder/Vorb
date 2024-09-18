#pragma once
#include "IWorldGenerationStage.h"
#include "world/biome/LivingBiomeType.h"

struct SimThreadEntityRequest;
class HostSimContext;
class AxisAlignedQuadMesh;
class MaterialShaderDef;
class World;

#ifdef DEBUG
constexpr ui64 HISTORY_GEN_DURATION_REAL_TIME_HOURS = 48; //48
#else
constexpr ui64 HISTORY_GEN_DURATION_REAL_TIME_HOURS = 48; //48
#endif

enum class HistoryEventType {
    ChernobogSpawn,
    BanshiraSpawn,
    COUNT
};

struct HistoryEvent {
    HistoryEventType type;
    f32 time;
};

// Allocates the world and generates history. While history generates,
// the sim thread is simulating biome growth and characters
class HistoryGenerationStage : public IWorldGenerationStage
{
public:
    HistoryGenerationStage(WorldDataGenerator& generator, const std::unique_ptr<World>& worldPtr);
    ~HistoryGenerationStage();

    void begin() override;

    const char* getStageName() const override { return "History"; }

    bool update() override;

    f32 getProgress() const override { return glm::min(mHistoryProgress, 1.0f); }

    void renderImguiControls() override;

private:

    // History events
    void handleHistoryEvent(HistoryEvent& event);
    void handleCorruptSpawn(LivingBiomeType type);
    
    // Old GPU Method
    //void downloadBiomes();

    f32 mHistoryProgress = 0.0f; // [0,1]
    TimestampMs mHistoryDurationMS = HISTORY_GEN_DURATION_REAL_TIME_HOURS * 60 * 60 * MS_PER_SECOND;
    ui32 mTickCount = 0;
    ui32 mNextEventIndex = 0;
    // Sorted by time
    std::vector<HistoryEvent> mHistoryEvents;

    RandomGenerator mRandomGen;

    const std::unique_ptr<World>& mWorldPtr;
    HostSimContext* mHostSimContext = nullptr;
};

