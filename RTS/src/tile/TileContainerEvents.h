#pragma once

#include "tile/Tile.h"

#include <variant>

class TileContainer;

enum class TileContainerEventType {
    LoadFinished,
    Activated,
    EditTiles,
    TileDamaged,
    TileDestroyed,
    Destroy,
};

enum class TileContainerEditEventType : ui8 {
    ChangeFlags       = BIT(0),
    ChangeLayer       = BIT(1),
    ChangeZPos        = BIT(2),
    ChangeOrientation = BIT(3),
    ChangeWall        = BIT(4),
    TYPES             = 5
};

struct TileContainerEditFlagsEventData {
    TileIndex tileIndex;
    f32v3 worldPosition;
    BitFlags<TileFlags> prevFlags;
    BitFlags<TileFlags> newFlags;
};

struct TileContainerEditLayerEventData {
    TileIndex tileIndex;
    f32v3 worldPosition;
    TileID prevId;
    TileID newId;
    ui8 newVariant;
    TileLayer layer;
};

struct TileContainerEditZPosEventData {
    TileIndex tileIndex;
    f32v3 worldPosition;
    f32 prevGroundZOffset;
    f32 newGroundZOffset;
};

struct TileContainerEditOrientationEventData {
    TileIndex tileIndex;
    f32v3 worldPosition;
    TileOrientation prevOrientation;
    TileOrientation newOrientation;
};

struct TileContainerEditEvent {
    union {
        TileContainerEditFlagsEventData* changeFlagsArray;
        TileContainerEditLayerEventData* changeLayerArray;
        TileContainerEditZPosEventData* changeZPosArray;
        TileContainerEditOrientationEventData* changeOrientationArray;
        // TODO: Walls
    };
    i32 editCount = 1;
    TileContainerEditEventType type;
};

struct TileDamagedEvent {
    TileIndex tileIndex = INVALID_TILE_INDEX;
    TileID tileId = TILE_ID_NONE;
    ui16 damageAmount = 0;
    f32v3 impactPosition = f32v3(FLT_MAX);
    f32v3 impactNormal = {};
    TileDamageData damageData;
    bool wasDestroyed = false;
};

struct TileContainerEvent {
    TileContainer* container = nullptr;
    std::variant<TileContainerEditEvent, TileDamagedEvent> varEvent;
};
EVENT_DISPATCHER_TYPE(TileContainer, TileContainerEventType, const TileContainerEvent&);

constexpr ui32 MAX_BULK_EDIT_EVENT_COUNT = CHUNK_SIZE;
