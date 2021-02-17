#pragma once

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


struct RoomNode {
    RoomTypeID nodeType;
    RoomNodeID parentRoom = INVALID_ROOM_ID; // Connected via door
    RoomNodeID childRooms[MAX_CHILD_ROOMS]; // Connected via doors, max of 4
    RoomNodeID adjacentRooms[MAX_CHILD_ROOMS]; // Like child rooms, connected via door or open wall, but is not a direct child
    RoomWall walls[MAX_WALLS_PER_ROOM]; // Starts at bottommost + leftmost, wall corner and proceeds in +y direction, then x,y,x,y,x, ect...
    ui16v2 offsetFromZero;
    ui16 size = 0;
    ui16 desiredSize = 0;
    ui16 desiredWidth = 0;
    ui8 numWalls = 0;
    ui8 numChildren = 0;
    bool isPrivate = false;
};
