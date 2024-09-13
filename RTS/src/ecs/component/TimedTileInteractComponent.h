#pragma once

struct TileRef;
struct TileHandle;

struct TimedTileInteractComponent {

    TimedTileInteractComponent(TileHandle interactTile, ui8 tileLayer, ui32 ticksUntilFinished, ui16 repeatCount, std::function<void(bool, TimedTileInteractComponent&)> callback = nullptr);
    ~TimedTileInteractComponent();

    VORB_NON_COPYABLE_BUT_MOVABLE(TimedTileInteractComponent);

    std::unique_ptr<TileRef> mInteractTile;
    TickCounter mTimer;
    ui16 mRepeatCount = 0;
    ui16 mCurrRepeat = 0;
    ui8 mTileLayer = 0;
    f32 mProgress = 0.0f;
    std::function<void(bool, TimedTileInteractComponent&)> mInteractFinishedCallback = nullptr;
};

class TimedTileInteractSystem {
public:
    TimedTileInteractSystem();

    void update(entt::registry& registry);
};