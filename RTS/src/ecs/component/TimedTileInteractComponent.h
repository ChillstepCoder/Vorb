#pragma once

#include "world/Chunk.h"

class World;

struct TimedTileInteractComponent {

    TimedTileInteractComponent(TileHandle interactTile, ui8 tileLayer, ui32 ticksUntilFinished, ui16 repeatCount, std::function<void(bool, TimedTileInteractComponent&)> callback = nullptr);

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
    TimedTileInteractSystem(World& world);

    void update(entt::registry& registry);

    World& mWorld;
};