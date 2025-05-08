#pragma once

#include "ui/minigame/FishingMinigame.h"

enum class MinigameType {
    Fishing,
    COUNT
};

// Manages, updates, and renders the current minigame for the local client
class LocalMinigameContext {
public:
    void updateAndRender(const f32v2 screenResolution, f32 elapsedSec);

    void abortCurrentMinigame();

    // TODO: Functor? https://stackoverflow.com/questions/18365532/should-i-pass-an-stdfunction-by-const-reference
    void beginFishingMinigame(const FishDef& fishData, OPT FishingMinigameGameThreadData* gameThreadData, std::function<void(const FishingMinigameResult& result)> onFinished);
    FishingMinigame* tryGetActiveFishingMinigame() const;
private:
    std::unique_ptr<ILocalMinigame> mCurrentMinigame = nullptr;

    MinigameType mMinigameType = MinigameType::COUNT;

    // We may start new minigames on game thread
    std::mutex mQueuedMingameLock;
    std::function<void()> mQueuedMinigameInit;
};

