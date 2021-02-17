#include "stdafx.h"
#include "BuildingBlueprintGenerator.h"

#include <Vorb/Timing.h>
#include "Random.h"

// Dont place on outer border, thats where facade goes
bool boundsCheckRoom(i16 pos, i16 dim) {
    return pos >= 1 && pos < dim - 1;
}


BuildingBlueprintGenerator::BuildingBlueprintGenerator(BuildingDescriptionRepository& buildingRepo) :
    mBuildingRepo(buildingRepo)
{

}

std::unique_ptr<BuildingBlueprint> BuildingBlueprintGenerator::generateBuilding(const BuildingDescription& desc, float sizeAlpha)
{
    PreciseTimer timer;

    std::unique_ptr<BuildingBlueprint> bp = std::make_unique<BuildingBlueprint>(desc, sizeAlpha);

    // Generate floorplan size
    bp->dims.x = vmath::lerp(desc.widthRange.x, desc.widthRange.y, sizeAlpha);
    // TODO: Dynamic aspect ratio
    bp->dims.y = bp->dims.x * desc.minAspectRatio;

    addPublicRoomsToGraph(*bp);
    assert(desc.publicRoomCountRange.y != 0.0f);
    assignPublicRooms(*bp);
    addPrivateRoomsToGraph(*bp);
    initRooms(*bp);
    placeRooms(*bp);
    expandRooms(*bp);

    std::cout << "\nGenerated building in " << timer.stop() << " ms\n";
    return bp;
}

void BuildingBlueprintGenerator::addPublicRoomsToGraph(BuildingBlueprint& bp) {
    // Generate public room structure using grammar
    ui32 publicRoomCount = bp.desc.publicRoomCountRange.y <= bp.desc.publicRoomCountRange.x ?
        bp.desc.publicRoomCountRange.x : Random::xorshf96() % (bp.desc.publicRoomCountRange.y - bp.desc.publicRoomCountRange.x) + bp.desc.publicRoomCountRange.x;
    assert(publicRoomCount); // Must have at least one public room
    bp.nodes.resize(publicRoomCount);
    bp.desc.publicGrammar.buildRoomGraph(bp.nodes);
}

void BuildingBlueprintGenerator::assignPublicRooms(BuildingBlueprint& bp)
{
    assert(bp.desc.publicRooms.size());
    ui8v2 countLookup[255]; // (current, max)
    ui32 publicRoomCount = bp.desc.publicRooms.size();
    ui32 availablePublicRooms = 0;

    { // Pre-pass set up count lookup
        ui32 roomIndex = 0;
        for (roomIndex = 0; roomIndex < publicRoomCount; ++roomIndex) {
            const PossibleRoom& room = bp.desc.publicRooms[roomIndex];
            countLookup[roomIndex].x = 0;
            countLookup[roomIndex].y = room.countRange.y;
            availablePublicRooms += room.countRange.y;
        }
    }
    assert(availablePublicRooms >= publicRoomCount);

    {// Generate rooms in order of priority while breadth first walking the tree
        ui32 roomIndex = 0;
        for (auto&& node : bp.nodes) {
            // Find a valid public room
            ui8v2* roomCount = &countLookup[roomIndex];
            while (roomCount->x >= roomCount->y) {
                // Wrap
                if (++roomIndex >= publicRoomCount) {
                    roomIndex = 0;
                }
                roomCount = &countLookup[roomIndex];
            }
            // Add this room
            ++roomCount->x;
            node.nodeType = bp.desc.publicRooms[roomIndex++].id;
            // Wrap
            if (roomIndex >= publicRoomCount) {
                roomIndex = 0;
            }
        }
    }
}

void BuildingBlueprintGenerator::addPrivateRoomsToGraph(BuildingBlueprint& bp) {
    const size_t numPublicRooms = bp.nodes.size();
    ui8v2 countLookup[255]; // (current, max)
    assert(numPublicRooms);
    size_t privateRoomCount = round(bp.sizeAlpha * (bp.desc.privateRoomCountRange.y - bp.desc.privateRoomCountRange.x) + bp.desc.privateRoomCountRange.x);
    if (!privateRoomCount) {
        return;
    }

    ui32 availablePrivateRooms = 0;

    { // Pre-pass set up count lookup
        ui32 roomIndex = 0;
        for (roomIndex = 0; roomIndex < bp.desc.privateRooms.size(); ++roomIndex) {
            const PossibleRoom& room = bp.desc.privateRooms[roomIndex];
            ui8 roomCount = vmath::roundApprox(vmath::lerp((f32)room.countRange.x, (f32)room.countRange.y, bp.sizeAlpha));
            countLookup[roomIndex].x = 0;
            countLookup[roomIndex].y = roomCount;
            availablePrivateRooms += roomCount;
        }
    }
    if (availablePrivateRooms < privateRoomCount) {
        privateRoomCount = availablePrivateRooms;
    }

    bp.nodes.reserve(bp.nodes.size() + privateRoomCount);
    int failCount = 0;
    int publicIndex = Random::xorshf96() % numPublicRooms;
    int privateIndex = 0;
    for (size_t i = 0; i < privateRoomCount; ++i) {
        RoomNode& publicRoom = bp.nodes[publicIndex];
        if (publicRoom.numChildren < MAX_CHILD_ROOMS && countLookup[privateIndex].x < countLookup[privateIndex].y) {
            // We can fit a private room here
            // Next node index is our child
            publicRoom.childRooms[publicRoom.numChildren++] = bp.nodes.size();
            // Append the room
            RoomNode privateRoom;
            privateRoom.nodeType = bp.desc.privateRooms[privateIndex].id;
            privateRoom.parentRoom = publicIndex;
            privateRoom.isPrivate = true;
            bp.nodes.emplace_back(std::move(privateRoom));
            // Limit our private count
            ++countLookup[privateIndex++].x;
            // Wrap
            if (privateIndex >= bp.desc.privateRooms.size()) {
                privateIndex = 0;
            }
            failCount = 0;
        }
        else {
            ++failCount;
        }
        if (failCount == numPublicRooms) {
            break;
        }

        // Next public room
        if (publicIndex == numPublicRooms - 1) {
            publicIndex = 0;
        }
        else {
            ++publicIndex;
        }
    }
}

void BuildingBlueprintGenerator::addStickOnRoomsToGraph()
{

}

ui16 getMaximumDepthRecursive(std::vector<RoomNode>& nodes, RoomNode* node) {
    ui16 maximumChildDepth = 0;
    for (int i = 0; i < node->numChildren; ++i) {
        ui16 depth = getMaximumDepthRecursive(nodes, &nodes[node->childRooms[i]]);
        if (depth > maximumChildDepth) {
            maximumChildDepth = depth;
        }
    }
    return maximumChildDepth + 1;
}

void placeChildrenRecursive(std::vector<RoomNode>& nodes, RoomNode* node, f32 availableYSpan, ui16 maxXOffsetPerLayer, ui16v2 currentOffset) {
    if (node->numChildren == 0) {
        return;
    }

    // TODO: Worry about even vs odd?
    const ui16 myDesiredRadius = node->desiredWidth / 2;

    // Determine desired child y span
    ui16 totalChildSpan = 0;
    for (int i = 0; i < node->numChildren; ++i) {
        RoomNode& child = nodes[node->childRooms[i]];
        totalChildSpan += child.desiredWidth;
    }

    const f32 desiredYSpan = vmath::min((f32)totalChildSpan, availableYSpan);

    // TODO: Dynamic child Y span based on available space?
    f32 childYSpan = desiredYSpan / node->numChildren;
    f32 ySegmentSize = desiredYSpan / (node->numChildren * 2);
    // Start at the top
    currentOffset.y -= ySegmentSize * (node->numChildren - 1);
    for (int i = 0; i < node->numChildren; ++i) {
        RoomNode& child = nodes[node->childRooms[i]];
        const ui16 childDesiredRadius = child.desiredWidth / 2;
        
        ui16 xOffset = vmath::min(maxXOffsetPerLayer, (ui16)(myDesiredRadius + childDesiredRadius));
        if (xOffset < 1) xOffset = 1;

        child.offsetFromZero = currentOffset;
        child.offsetFromZero.x += xOffset;
        placeChildrenRecursive(nodes, &child, childYSpan, maxXOffsetPerLayer, child.offsetFromZero);
        currentOffset.y += ySegmentSize * 2;
    }
}

void BuildingBlueprintGenerator::initRooms(BuildingBlueprint& bp) {
    for (auto&& room : bp.nodes) {
        RoomDescription& desc = mBuildingRepo.getRoomDescriptionFromID(room.nodeType);
        room.desiredWidth = round(lerp(desc.minWidth, desc.maxWidth, bp.sizeAlpha));
        room.desiredSize = room.desiredWidth * room.desiredWidth; //SQ
    }
}

void applyForceOffset(ui16v2& offset, const f32v2& force, const ui16v2& dims) {
    i32v2 newOffset = i32v2(offset) + i32v2(force);
    offset.x = vmath::clamp(newOffset.x, 1, dims.x - 2);
    offset.y = vmath::clamp(newOffset.y, 1, dims.y - 2);
}

void BuildingBlueprintGenerator::placeRooms(BuildingBlueprint& bp) {
    // Breadth first search room placement
    RoomNode* root = &bp.nodes[0];

    ui16 maximumDepth = getMaximumDepthRecursive(bp.nodes, root);

    ui16 maxXOffsetPerLayer = bp.dims.x / maximumDepth;
    f32 availableYSpan = bp.dims.y;
    // First place the root
    root->offsetFromZero = i16v2(vmath::min(maxXOffsetPerLayer / 2, root->desiredWidth / 2), bp.dims.y / 2);
    if (root->offsetFromZero.x == 0) root->offsetFromZero.x = 1u;

    placeChildrenRecursive(bp.nodes, root, availableYSpan, maxXOffsetPerLayer, root->offsetFromZero);

    // Clamp positions to be withing facade
    for (auto&& room : bp.nodes) {
        room.offsetFromZero.x = vmath::clamp(room.offsetFromZero.x, (ui16)1u, bp.dims.x);
        room.offsetFromZero.y = vmath::clamp(room.offsetFromZero.y, (ui16)1u, bp.dims.y);
    }

    // Spread rooms apart based on circular collision
    constexpr f32 FORCE_MULT = 0.5f;
    for (int iter = 0; iter < 3; ++iter) {
        for (size_t i = 0; i < bp.nodes.size() - 1; ++i) {
            RoomNode& room1 = bp.nodes[i];
            for (size_t j = i + 1; j < bp.nodes.size(); ++j) {
                RoomNode& room2 = bp.nodes[j];

                f32v2 offset;
                // Offset should never be 0
                if (room1.offsetFromZero != room2.offsetFromZero) {
                    offset = f32v2(room2.offsetFromZero) - f32v2(room1.offsetFromZero);
                }
                else {
                    offset = f32v2(1.0f, 0.0f);
                }
                const f32 distance = glm::length(offset);
                // Normalize
                offset = offset / distance;
                const f32 desiredDistance = (room1.desiredWidth + room2.desiredWidth) / 2.0f;
                // Collide with everything
                if (distance < desiredDistance) {
                    const f32v2 pushForce = offset * ((desiredDistance - distance) * FORCE_MULT);
                    applyForceOffset(room1.offsetFromZero, -pushForce, bp.dims);
                    applyForceOffset(room2.offsetFromZero, pushForce, bp.dims);
                } else if (room2.parentRoom == i) { // Magnet only to children
                    const f32v2 pullForce = offset * ((distance - desiredDistance) * FORCE_MULT);
                    applyForceOffset(room1.offsetFromZero, pullForce, bp.dims);
                    applyForceOffset(room2.offsetFromZero, -pullForce, bp.dims);

                }
            }
        }
    }

    // Second spread pass aiming only at tiny distances
    for (int iter = 0; iter < 2; ++iter) {
        for (size_t i = 0; i < bp.nodes.size() - 1; ++i) {
            RoomNode& room1 = bp.nodes[i];
            for (size_t j = i + 1; j < bp.nodes.size(); ++j) {
                RoomNode& room2 = bp.nodes[j];

                f32v2 offset;
                // Offset should never be 0
                if (room1.offsetFromZero != room2.offsetFromZero) {
                    offset = f32v2(room2.offsetFromZero) - f32v2(room1.offsetFromZero);
                }
                else {
                    offset = f32v2(1.0f, 0.0f);
                }
                const f32 distance = glm::length(offset);
                // Normalize
                offset = offset / distance;
                const f32 desiredDistance = (room1.desiredWidth + room2.desiredWidth) / 2.0f;
                // Collide with everything
                if (distance < desiredDistance && distance < 3) {
                    const f32v2 pushForce = offset * ((desiredDistance - distance));
                    applyForceOffset(room1.offsetFromZero, -pushForce, bp.dims);
                    applyForceOffset(room2.offsetFromZero, pushForce, bp.dims);
                }
            }
        }
    }
}

// Helper
ui16 getIndexAtPos(ui16v2 pos, ui16 xDims) {
    return pos.y * xDims + pos.x;
}

f32 getPressureValue(ui16 desiredSize, ui16 currentSize) {
    return (f32)desiredSize / (f32)currentSize;
}



constexpr ui8 ITER_STEP = 2;
constexpr ui8 MAX_WALL_LENGTH = 64;

i16v2 EXPAND_OFFSETS[4] = {
    {-1,  0}, // LEFT
    { 0,  1}, // TOP
    { 1,  0}, // RIGHT
    { 0, -1}  // BOTTOM
};

i16v2 ITERATE_OFFSETS[4] = {
    { 0,  1}, // LEFT
    { 1,  0}, // TOP
    { 0, -1}, // RIGHT
    {-1,  0}  // BOTTOM
};

RoomWallOuterDir OPPOSITE_WALL_DIRS[4] = {
    RoomWallOuterDir::RIGHT,  // LEFT
    RoomWallOuterDir::BOTTOM, // TOP
    RoomWallOuterDir::LEFT,   // RIGHT
    RoomWallOuterDir::TOP     // BOTTOM
};

void extendWallStart(RoomWall& wall, const i16v2& offset) {
    wall.startPos += offset;
    ++wall.length;
}

void extendWallEnd(RoomWall& wall, const i16v2& offset) {
    wall.endPos += offset;
    ++wall.length;
}


void expandWall(RoomWall& wall, BuildingBlueprint& bp, RoomNode& room, RoomNodeID roomId, std::vector<RoomNodeID>& metaData) {
    const i16v2& expandOffset = EXPAND_OFFSETS[enum_cast(wall.outerDir)];
    const i16v2& iterateOffset = ITERATE_OFFSETS[enum_cast(wall.outerDir)];

    wall.startPos += expandOffset;
    wall.endPos += expandOffset;
    extendWallEnd(*wall.startAdjacent, expandOffset);
    extendWallStart(*wall.endAdjacent, expandOffset);

    // Set new metadata
    i16v2 outerPos = wall.startPos;
    for (int j = 0; j < wall.length; ++j) {
        // Get owner room and determine pressure contribution
        const ui16 index = getIndexAtPos(outerPos, bp.dims.x);
        RoomNodeID ownerId = metaData[index];
        if (ownerId != INVALID_ROOM_ID) {
            RoomNode& ownerRoom = bp.nodes[ownerId];
            // TODO: Can we optimize this so we don't run it every time?
        }
        metaData[index] = roomId;
        bp.tiles[index].type = BlueprintTileType::FLOOR;
        // Step
        outerPos += iterateOffset;
    }

    // TODO: Only along actual expansion tiles
    // TODO: Decrement size when we push into another room
    room.size += wall.length;
}

// Only fills gaps and will not overwrite any existing walls
void expandWallGapsOnly(RoomWall& wall, BuildingBlueprint& bp, RoomNode& room, RoomNodeID roomId, std::vector<RoomNodeID>& metaData) {
    const i16v2& expandOffset = EXPAND_OFFSETS[enum_cast(wall.outerDir)];
    const i16v2& iterateOffset = ITERATE_OFFSETS[enum_cast(wall.outerDir)];

    wall.startPos += expandOffset;
    wall.endPos += expandOffset;
    // TODO: This is invalid as it will expand 1 bits into owned territory?
    // TODO: Can we delete the bit array with new method?
    extendWallEnd(*wall.startAdjacent, expandOffset);
    extendWallStart(*wall.endAdjacent, expandOffset);

    // Set new metadata
    i16v2 outerPos = wall.startPos;
    ui16 sizeAdd = 0;
    for (int j = 0; j < wall.length; ++j) {
        // Only if we aren't an overwritten wall
        // Get owner room and determine pressure contribution
        const ui16 index = getIndexAtPos(outerPos, bp.dims.x);
        RoomNodeID ownerId = metaData[index];
        if (ownerId == INVALID_ROOM_ID) {
            ++sizeAdd;
            metaData[index] = roomId;
            bp.tiles[index].type = BlueprintTileType::FLOOR;
        }
        // Step
        outerPos += iterateOffset;
    }

    // TODO: Only along actual expansion tiles
    // TODO: Decrement size when we push into another room
    room.size += wall.length;
}

bool expandRoomSquare(BuildingBlueprint& bp, RoomNode& room, RoomNodeID roomId, std::vector<RoomNodeID>& metaData) {

   bool didExpand = false;

   for (int iter = 0; iter < ITER_STEP; ++iter) {
       // Try expand walls
       int expandCount = 0;
       for (int i = 0; i < room.numWalls; ++i) {

           f32 currentPressure = getPressureValue(room.desiredSize, room.size);
           // Once we are at desired size, no need to expand
           if (currentPressure <= 1.0f) {
               return didExpand;
           }

           RoomWall& wall = room.walls[i];
           // Expand
           const i16v2& expandOffset = EXPAND_OFFSETS[enum_cast(wall.outerDir)];
           const i16v2& iterateOffset = ITERATE_OFFSETS[enum_cast(wall.outerDir)];
           const int xOrY = (int)wall.outerDir % 2;
           const i16v2 nextStart = wall.startPos + expandOffset;
           // Bounds check
           if (boundsCheckRoom(nextStart[xOrY], bp.dims[xOrY])) {
               assert(wall.length <= MAX_WALL_LENGTH);
               // We will only expand if we arent expanding into another room
               bool canExpand = true;
               i16v2 outerPos = nextStart;
               for (int j = 0; j < wall.length; ++j) {
                   const ui16 index = getIndexAtPos(outerPos, bp.dims.x);
                   RoomNodeID ownerId = metaData[index];
                   if (ownerId != INVALID_ROOM_ID) {
                       canExpand = false;
                       break;
                   }
                   // Step
                   outerPos += iterateOffset;
               }
               // If we have room to expand, expand
               if (canExpand) {
                   expandWall(wall, bp, room, roomId, metaData);
                   didExpand = true;
                   ++expandCount;
               }
           }
       }
       // Early out due to no expansion
       if (expandCount == 0) {
           return didExpand;
       }
   }
   return didExpand;
}

bool expandRoomGaps(BuildingBlueprint& bp, RoomNode& room, RoomNodeID roomId, std::vector<RoomNodeID>& metaData) {

    bool didExpand = false;

    for (int iter = 0; iter < ITER_STEP; ++iter) {

        // Try expand walls
        int expandCount = 0;
        for (int i = 0; i < room.numWalls; ++i) {

            f32 currentPressure = getPressureValue(room.desiredSize, room.size);
            // Once we are at desired size, no need to expand
            if (currentPressure <= 1.0f) {
                return didExpand;
            }

            RoomWall& wall = room.walls[i];
            // Expand
            const i16v2& expandOffset = EXPAND_OFFSETS[enum_cast(wall.outerDir)];
            const i16v2& iterateOffset = ITERATE_OFFSETS[enum_cast(wall.outerDir)];
            const int xOrY = (int)wall.outerDir % 2;
            const i16v2 nextStart = wall.startPos + expandOffset;
            // Bounds check
            if (boundsCheckRoom(nextStart[xOrY], bp.dims[xOrY])) {
                assert(wall.length <= MAX_WALL_LENGTH);
                // We will only expand if there is free space
                bool canExpand = false;
                i16v2 outerPos = nextStart;
                for (int j = 0; j < wall.length; ++j) {
                    // We will expand if there is at least one empty square here
                    const ui16 index = getIndexAtPos(outerPos, bp.dims.x);
                    RoomNodeID ownerId = metaData[index];
                    if (ownerId == INVALID_ROOM_ID) {
                        canExpand = true;
                        break;
                    }
                    // Step
                    outerPos += iterateOffset;
                }
                // If we have room to expand, expand
                if (canExpand) {
                    expandWallGapsOnly(wall, bp, room, roomId, metaData);
                    didExpand = true;
                    ++expandCount;
                }
            }
        }
        // Early out due to no expansion
        if (expandCount == 0) {
            return didExpand;
        }
    }
    return didExpand;
}

void placeFacadeTiles(BuildingBlueprint& bp, RoomNode& room, RoomNodeID roomId, std::vector<RoomNodeID>& metaData) {
    // Iteratively expand walls
    for (int i = 0; i < room.numWalls; ++i) {
        RoomWall& wall = room.walls[i];
        // Opposite for iteration axis
        const int xOrY = ((int)wall.outerDir + 1) % 2;
        // Deliberately is 1 less than the length
        int wallLength = abs(wall.endPos[xOrY] - wall.startPos[xOrY]);
        const i16v2& iterDir = ITERATE_OFFSETS[enum_cast(wall.outerDir)];
        const i16v2& outerDir = EXPAND_OFFSETS[enum_cast(wall.outerDir)];
        i16v2 pos = wall.startPos;
        assert(pos.x >= 0 && pos.y >= 0);
        bool finalWasSuccess = false;
        // Iterate along the wall and mark as wall nodes
        for (int j = 0; j <= wallLength; ++j) {
            ui32 index = pos.y * bp.dims.x + pos.x;
            // Only place wall if we own this tile
            if (metaData[index] == roomId) {
                i16v2 facadePos = pos + outerDir;
                ui32 facadeIndex = facadePos.y * bp.dims.x + facadePos.x;
                // Only place facade if this is an unowned tile
                if (metaData[facadeIndex] == INVALID_ROOM_ID) {
                    bp.tiles[facadeIndex].type = BlueprintTileType::WALL;
                    finalWasSuccess = true;
                }
                else {
                    finalWasSuccess = false;
                }
            }
            else {
                finalWasSuccess = false;
            }
            pos += iterDir;
        }
        // If last tile was successful, do one more to place the corner piece
        if (finalWasSuccess) {
            i16v2 facadePos = pos + outerDir;
            ui32 facadeIndex = facadePos.y * bp.dims.x + facadePos.x;
            // Only place facade if this is an unowned tile
            if (metaData[facadeIndex] == INVALID_ROOM_ID) {
                bp.tiles[facadeIndex].type = BlueprintTileType::WALL;
            }
        }
    }
}

void placeInteriorWalls(BuildingBlueprint& bp, std::vector<RoomNodeID>& metaData) {
    for (ui16 y = 1; y < bp.dims.y - 1; ++y) {
        for (ui16 x = 1; x < bp.dims.x - 1; ++x) {
            ui16 index = y * bp.dims.x + x;
            RoomNodeID roomId = metaData[index];
            if (roomId == INVALID_ROOM_ID) {
                continue;
            }
            { // Right
                ui16 nIndex = index + 1;
                RoomNodeID nId = metaData[nIndex];
                if (nId != INVALID_ROOM_ID && nId != roomId) {
                    bp.tiles[nIndex].type = BlueprintTileType::WALL;
                }
            }
            { // Down
                ui16 nIndex = index - bp.dims.x;
                RoomNodeID nId = metaData[nIndex];
                if (nId != INVALID_ROOM_ID && nId != roomId) {
                    bp.tiles[nIndex].type = BlueprintTileType::WALL;
                }
            }
            { // Down Right
                ui16 nIndex = index + 1 - bp.dims.x;
                RoomNodeID nId = metaData[nIndex];
                if (nId != INVALID_ROOM_ID && nId != roomId) {
                    bp.tiles[nIndex].type = BlueprintTileType::WALL;
                }
            }
        }
    }
}

void BuildingBlueprintGenerator::expandRooms(BuildingBlueprint& bp) {
    bp.tiles.resize((size_t)bp.dims.x * (size_t)bp.dims.y, { BlueprintTileType::NONE });
    std::vector<RoomNodeID> metaData(bp.tiles.size(), INVALID_ROOM_ID);

    // Init rooms
    for (size_t i = 0; i < bp.nodes.size(); ++i) {
        initRoomWalls(bp, bp.nodes[i], i, metaData);
    }

    // Expand walls in square shape, no overwrite
    for (int iters = 0; iters < MAX_WALL_LENGTH / ITER_STEP; ++iters) {
        int failCount = 0;
        for (size_t i = 0; i < bp.nodes.size(); ++i) {
            failCount += expandRoomSquare(bp, bp.nodes[i], i, metaData) ? 0 : 1;
        }
        if (failCount == bp.nodes.size()) {
            break;
        }
    }
    // Fill in gaps, no overwrite
    for (int iters = 0; iters < MAX_WALL_LENGTH / ITER_STEP; ++iters) {
        int failCount = 0;
        for (size_t i = 0; i < bp.nodes.size(); ++i) {
            failCount += expandRoomGaps(bp, bp.nodes[i], i, metaData) ? 0 : 1;
        }
        if (failCount == bp.nodes.size()) {
            break;
        }
    }

    // Place facade tiles
    for (size_t i = 0; i < bp.nodes.size(); ++i) {
        placeFacadeTiles(bp, bp.nodes[i], i, metaData);
    }

    // Place interior wall tiles
    placeInteriorWalls(bp, metaData);

    // TODO: Debug only
    bp.ownerArray.swap(metaData);
}

void BuildingBlueprintGenerator::initRoomWalls(BuildingBlueprint& bp, RoomNode& room, RoomNodeID roomId, std::vector<RoomNodeID>& metaData)
{
    ui16 index = getIndexAtPos(room.offsetFromZero, bp.dims.x);
    // Init root node
    room.size = 1;
    bp.tiles[index].type = BlueprintTileType::FLOOR;
    metaData[index] = roomId;

    // Init 4 base walls
    room.numWalls = 4;
    for (int i = 0; i < room.numWalls; ++i) {
        RoomWall& wall = room.walls[i];
        wall.startPos = room.offsetFromZero;
        wall.endPos = room.offsetFromZero;
        wall.length = 1;
    }
    room.walls[(int)RoomWallOuterDir::LEFT].startAdjacent = &room.walls[(int)RoomWallOuterDir::BOTTOM];
    room.walls[(int)RoomWallOuterDir::LEFT].endAdjacent = &room.walls[(int)RoomWallOuterDir::TOP];
    room.walls[(int)RoomWallOuterDir::TOP].startAdjacent = &room.walls[(int)RoomWallOuterDir::LEFT];
    room.walls[(int)RoomWallOuterDir::TOP].endAdjacent = &room.walls[(int)RoomWallOuterDir::RIGHT];
    room.walls[(int)RoomWallOuterDir::RIGHT].startAdjacent = &room.walls[(int)RoomWallOuterDir::TOP];
    room.walls[(int)RoomWallOuterDir::RIGHT].endAdjacent = &room.walls[(int)RoomWallOuterDir::BOTTOM];
    room.walls[(int)RoomWallOuterDir::BOTTOM].startAdjacent = &room.walls[(int)RoomWallOuterDir::RIGHT];
    room.walls[(int)RoomWallOuterDir::BOTTOM].endAdjacent = &room.walls[(int)RoomWallOuterDir::LEFT];

    room.walls[(int)RoomWallOuterDir::LEFT].outerDir = RoomWallOuterDir::LEFT;
    room.walls[(int)RoomWallOuterDir::TOP].outerDir = RoomWallOuterDir::TOP;
    room.walls[(int)RoomWallOuterDir::RIGHT].outerDir = RoomWallOuterDir::RIGHT;
    room.walls[(int)RoomWallOuterDir::BOTTOM].outerDir = RoomWallOuterDir::BOTTOM;
}

void BuildingBlueprintGenerator::placeDoors(BuildingBlueprint& bp) {

}
