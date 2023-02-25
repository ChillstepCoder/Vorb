#pragma once

#include "tile/Tile.h"

class TileContainer;

enum class TileContainerEventType {
    Ready,
    EditTiles,
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
    ui32 editCount = 1;
    TileContainerEditEventType type;
};

struct TileContainerEvent {
    TileContainer* container = nullptr;
    TileContainerEditEvent edit = {}; // TODO: Union
};
EVENT_DISPATCHER_TYPE(TileContainer, TileContainerEventType, const TileContainerEvent&);

constexpr ui32 MAX_BULK_EDIT_EVENT_COUNT = 2048;
