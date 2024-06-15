#pragma once

#include "ecs/AttachedEntityUpdater.h"
#include "item/ItemStack.h"
#include "pathfinding/NavPath.h"


class Camera3D;

struct GameplayDebugOptions {
    bool showAIDebugger = true;
};
// Managed by GameplayDebugger singleton
inline static GameplayDebugOptions sGameplayDebugOptions;

// TODO: Put in cpp file and remove above includes
struct AIDebugData {
    std::string tasksDebugString;
    BuildingID homeId = INVALID_BUILDING_ID;
    f32v3 position = f32v3(0.0f);
    SimpleItemStack bundle;
    std::shared_ptr<NavPath> finePath;
    std::shared_ptr<NavPath> coarsePath;
    i32 currentFineNode = 0;
    i32 currentCoarseNode = 0;
    bool showPaths = false;
};

using AIDebugEntityUpdateHandle = AttachedEntityUpdateHandle<AIDebugData>;
using AIDebugEntityUpdateHandlePtr = std::shared_ptr<AIDebugEntityUpdateHandle>;

// TODO: New file
class AIDebugger {
public:
    AIDebugger();
    ~AIDebugger();

    void updateAndRenderImGui(const Camera3D& camera);
private:

    std::vector<AIDebugEntityUpdateHandlePtr> mEntityUpdateHandles;
};

// Singleton managed by UIContext
class GameplayDebugger
{
public:
    GameplayDebugger();
    ~GameplayDebugger();
    
    GameplayDebugger* tryGetInstance();
    AIDebugger& getAIDebugger() { return mAIDebugger; }

public:
    void updateAndRenderImGui(const Camera3D& camera);
    const GameplayDebugOptions& getOptions() const { return sGameplayDebugOptions; }
private:

    AIDebugger mAIDebugger;
};

