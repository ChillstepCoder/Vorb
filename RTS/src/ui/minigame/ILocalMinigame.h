#pragma once


enum class MinigameResultType {
    InProgress,
    Fail,
    Success,
    COUNT
};

struct MinigameResult {
    MinigameResultType mType = MinigameResultType::InProgress;
    int mExtraResultData = 0;
};

// Represents a UI renderable minigame for the local client
class ILocalMinigame {
public:
    ILocalMinigame() = default;
    virtual ~ILocalMinigame() = default;

    virtual MinigameResult updateAndRender(const f32v2 screenResolution, f32 elapsedSec) = 0;
    virtual void abort() = 0;
};