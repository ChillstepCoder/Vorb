#pragma once

// Describes a building graph
struct RoomNode {
    RoomTypeID nodeType;
    RoomNodeID parentRoom = INVALID_ROOM_ID; // Connected via door
    RoomNodeID childRooms[MAX_CHILD_ROOMS]; // Connected via doors, max of 4
    RoomNodeID adjacentRooms[MAX_CHILD_ROOMS]; // Like child rooms, connected via door or open wall, but is not a direct child
    ui16 walls[MAX_WALLS_PER_ROOM]; // Starts at bottommost + leftmost, wall corner and proceeds in +y direction, then x,y,x,y,x, ect...
    i16v2 offsetFromRoot;
    ui8 numWalls = 0;
    ui8 numChildren = 0;
    bool isPrivate = false;
};
