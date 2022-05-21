#pragma once

struct RoomDef;

// Describes initial four walls
enum class RoomWallOuterDir {
    LEFT,
    TOP,
    RIGHT,
    BOTTOM
};

// Describes a building graph
struct RoomWall {
    i16v2 startPos;
    i16v2 endPos;
    RoomWall* startAdjacent;
    RoomWall* endAdjacent;
    RoomWallOuterDir outerDir;
    ui8 length;
};

struct RoomGateInfo {
    RoomNodeID adjacentRoom;
    ui32 tileIndex;
};

struct RoomNode {
    RoomDefID roomDefId;
    RoomNodeID parentRoom = INVALID_ROOM_ID; // Connected via door or stairs
    RoomNodeID childRooms[MAX_CHILD_ROOMS]; // Connected via door or stairs, max of 4
    RoomGateInfo adjacentRooms[MAX_CHILD_ROOMS]; // Like child rooms, connected via door or open wall, but is not a direct child
    RoomNodeID id = INVALID_ROOM_ID;
    RoomWall walls[MAX_WALLS_PER_ROOM]; // Starts at bottommost + leftmost, wall corner and proceeds in +y direction, then x,y,x,y,x, ect...
    ui32AABB2 aabb = { 0 };
    ui16v2 offsetFromZero;
    ui16 size = 0;
    ui16 desiredSize = 0;
    ui16 desiredWidth = 0;
    ui16 floorIndex = 0;
    ui8 numWalls = 0;
    ui8 numChildren = 0;
    ui8 numAdjacentRooms = 0;
    bool isPrivate = false;
    const RoomDef* roomDef = nullptr;
};