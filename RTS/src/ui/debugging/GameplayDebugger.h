#pragma once

#include "ecs/AttachedEntityUpdater.h"

class Camera3D;

struct GameplayDebugOptions {
    bool showAIDebugger = false;
};
// Managed by GameplayDebugger singleton
inline static GameplayDebugOptions sGameplayDebugOptions;

struct AIDebugData {
    StrToken taskName;
    f32v3 position;
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

