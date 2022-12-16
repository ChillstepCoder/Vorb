#pragma once

class TileContainer;

enum class TileContainerEventType {
    Ready,
    EditTile,
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
    BitFlags<TileFlags> prevFlags;
    BitFlags<TileFlags> newFlags;
};

struct TileContainerEditLayerEventData {
    TileID prevId;
    TileID newId;
    TileLayer layer;
};

struct TileContainerEditZPosEventData {
    f32 prevGroundZOffset;
    f32 newGroundZOffset;
};

struct TileContainerEditOrientationEventData {
    TileOrientation prevOrientation;
    TileOrientation newOrientation;
};

struct TileContainerEditEvent {
    TileIndex editPosition;
    TileContainerEditEventType type;
    union {
        TileContainerEditFlagsEventData changeFlags;
        TileContainerEditLayerEventData changeLayer;
        TileContainerEditZPosEventData changeZPos;
        TileContainerEditOrientationEventData changeOrientation;
        // TODO: Walls
    };
};

struct TileContainerEvent {
    TileContainer* container = nullptr;
    TileContainerEditEvent edit = {}; // TODO: Union
};