#pragma once

struct RoomDef;
class Building;

#include "util/GridEdge.h"
#include "tile/Stairs.h"

struct RoomGateInfo {
    RoomNodeID adjacentRoom;
    ui32 tileIndex;
};



struct RoomNode {
    std::vector<GridEdge> interiorEdges;
    std::vector<TileIndex> edgeWalk;
    const RoomDef* roomDef = nullptr;
    RoomDefID roomDefId;
    RoomNodeID parentRoom = INVALID_ROOM_ID; // Connected via door or stairs
    RoomNodeID childRooms[MAX_CHILD_ROOMS]; // Connected via door or stairs, max of 4
    RoomGateInfo adjacentRooms[MAX_ADJACENT_ROOMS]; // Like child rooms, connected via door or open wall, but is not necessarily a direct child, also includes exterior doors
    RoomNodeID id = INVALID_ROOM_ID;
    i32AABB2 aabb = { 0 };
    ui16v2 offsetFromZero;
    ui16 size = 0;
    ui16 desiredSize = 0;
    ui16 desiredWidth = 0;
    ui16 floorIndex = 0;
    ui8 numChildren = 0;
    ui8 numAdjacentRooms = 0;
    bool isPrivate = false;
    bool hasStairs = false;
    bool connectedToParentWithStairs = false;
};