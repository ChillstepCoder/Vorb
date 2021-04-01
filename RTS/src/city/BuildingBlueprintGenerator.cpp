#include "stdafx.h"
#include "BuildingBlueprintGenerator.h"

#include "services/Services.h"

#include <Vorb/Timing.h>
#include "Random.h"

// Dont place on outer border, thats where facade goes
bool boundsCheckRoom(i16 pos, i16 dim) {
    return pos >= 1 && pos < dim - 1;
}


BuildingBlueprint::BuildingBlueprint(
    const BuildingDescription& desc,
    float sizeAlpha,
    Cartesian entrySide,
    ui16v2 dims,
    ui32v2 bottomLeftWorldPos
) :
    desc(desc), sizeAlpha(sizeAlpha), entrySide(entrySide), dims(dims), bottomLeftWorldPos(bottomLeftWorldPos) {

}



BuildingBlueprintGenerator::BuildingBlueprintGenerator(BuildingDescriptionRepository& buildingRepo) :
    mBuildingRepo(buildingRepo)
{

}

std::unique_ptr<BuildingBlueprint> BuildingBlueprintGenerator::generateBuildingAsync(const BuildingDescription& desc, float sizeAlpha, Cartesian entrySide, ui16v2 plotSize, const ui32v2& bottomLeftPos)
{
    assert(desc.publicRoomCountRange.y != 0.0f);
    ++mCurrentId;
    // Will this ever happen? maybe...
    if (mCurrentId == INVALID_BLUEPRINT_ID) {
        mCurrentId = 0;
    }

    std::unique_ptr<BuildingBlueprint> bp = std::make_unique<BuildingBlueprint>(desc, sizeAlpha, entrySide, plotSize, bottomLeftPos);
    bp->id = mCurrentId;
    BuildingBlueprint* bPtr = bp.get();
    mGeneratingBuildings.insert(bPtr);
    
    Services::Threadpool::ref().addTask([&, bPtr](ThreadPoolWorkerData* workerData) {

        // Room Graph
        addPublicRoomsToGraph(*bPtr);
        assignPublicRooms(*bPtr);
        addPrivateRoomsToGraph(*bPtr);

        // Rooms
        initRooms(*bPtr);
        placeRooms(*bPtr);
        expandRooms(*bPtr);
        roomCleanup(*bPtr);

        // Walls
        placeFacadeWalls(*bPtr);
        placeInteriorWalls(*bPtr);

        // Doors
        // placeHallwayDoors
        placeDoors(*bPtr);

        // Furniture

        // Flooring
    }, [&, bPtr]() {
        // Main thread
        mGeneratingBuildings.erase(mGeneratingBuildings.find(bPtr));
        bPtr->isGenerating = false;
    });
    return bp;
}

void BuildingBlueprintGenerator::addPublicRoomsToGraph(BuildingBlueprint& bp) const {
    // Generate public room structure using grammar
    ui32 publicRoomCount = bp.desc.publicRoomCountRange.y <= bp.desc.publicRoomCountRange.x ?
        bp.desc.publicRoomCountRange.x : Random::xorshf96() % (bp.desc.publicRoomCountRange.y - bp.desc.publicRoomCountRange.x) + bp.desc.publicRoomCountRange.x;
    assert(publicRoomCount); // Must have at least one public room
    bp.nodes.resize(publicRoomCount);
    bp.desc.publicGrammar.buildRoomGraph(bp.nodes);
}

void BuildingBlueprintGenerator::assignPublicRooms(BuildingBlueprint& bp) const
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

void BuildingBlueprintGenerator::addPrivateRoomsToGraph(BuildingBlueprint& bp) const {
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

void BuildingBlueprintGenerator::addStickOnRoomsToGraph() const
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

void placeChildrenRecursive(std::vector<RoomNode>& nodes, RoomNode* node, f32 availableWidthSpan, ui16 maxXOffsetPerLayer, ui16v2 currentOffset) {
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

    const f32 desiredWidthSpan = vmath::min((f32)totalChildSpan, availableWidthSpan);

    // TODO: Dynamic child Y span based on available space?
    f32 childWidthSpan = desiredWidthSpan / node->numChildren;
    f32 widthSegmentSize = desiredWidthSpan / (node->numChildren * 2);
    // Start at the top
    currentOffset.y -= widthSegmentSize * (node->numChildren - 1);
    for (int i = 0; i < node->numChildren; ++i) {
        RoomNode& child = nodes[node->childRooms[i]];
        const ui16 childDesiredRadius = child.desiredWidth / 2;
        
        ui16 xOffset = vmath::min(maxXOffsetPerLayer, (ui16)(myDesiredRadius + childDesiredRadius));
        if (xOffset < 1) xOffset = 1;

        child.offsetFromZero = currentOffset;
        child.offsetFromZero.x += xOffset;
        placeChildrenRecursive(nodes, &child, childWidthSpan, maxXOffsetPerLayer, child.offsetFromZero);
        currentOffset.y += widthSegmentSize * 2;
    }
}

void BuildingBlueprintGenerator::initRooms(BuildingBlueprint& bp) const {
    for (size_t i = 0; i < bp.nodes.size(); ++i) {
        RoomNode& room = bp.nodes[i];
        room.id = i;

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

void BuildingBlueprintGenerator::placeRooms(BuildingBlueprint& bp) const {

    // Breadth first search room placement
    RoomNode* root = &bp.nodes[0];
    ui16 maximumDepth = getMaximumDepthRecursive(bp.nodes, root);

    // Determine which dims to use for cartesian
    ui16v2 dims;
    switch (bp.entrySide) {
        case Cartesian::DOWN:
        case Cartesian::UP:
            dims.x = bp.dims.y;
            dims.y = bp.dims.x;
            break;
        case Cartesian::LEFT:
        case Cartesian::RIGHT:
            dims = bp.dims;
            break;
    }
    ui16 maxDepthOffsetPerLayer = dims.x / maximumDepth;
    f32 availableWidthSpan = dims.y;
    // Place the root
    root->offsetFromZero = i16v2(vmath::min(maxDepthOffsetPerLayer / 2, root->desiredWidth / 2), dims.y / 2);
    if (root->offsetFromZero.x == 0) root->offsetFromZero.x = 1u;

    // We will generate to the right, then will rotate the coordinates around based on the cartesian
    placeChildrenRecursive(bp.nodes, root, availableWidthSpan, maxDepthOffsetPerLayer, root->offsetFromZero);

    // Rotate all coordinates around for Cartesian direction
    // Left is the base case so do nothing for that
    i32v2 cartesianDirection;
    switch (bp.entrySide) {
        case Cartesian::DOWN:
            for (auto&& room : bp.nodes) {
                ui16 tmp = room.offsetFromZero.x;
                room.offsetFromZero.x = room.offsetFromZero.y;
                room.offsetFromZero.y = bp.dims.y - tmp - 1;
            }
            break;
        case Cartesian::RIGHT:
            for (auto&& room : bp.nodes) {
                room.offsetFromZero.x = bp.dims.x - room.offsetFromZero.x - 1;
            }
            break;
        case Cartesian::UP:
            for (auto&& room : bp.nodes) {
                std::swap(room.offsetFromZero.x, room.offsetFromZero.y);
                room.offsetFromZero.x = bp.dims.x - room.offsetFromZero.x - 1;
            }
            break;
    }

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
inline ui16 getIndexAtPos(ui16v2 pos, ui16 xDims) {
    return pos.y * xDims + pos.x;
}

inline i16 getIndexAtPos(i16v2 pos, ui16 xDims) {
    return pos.y * xDims + pos.x;
}

inline ui32 getIndexAtPos(ui32 x, ui32 y, ui32 xDims) {
    return y * xDims + x;
}

inline f32 getPressureValue(const RoomNode& room) {
    return (f32)room.desiredSize / (f32)room.size;
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


void expandWall(RoomWall& wall, BuildingBlueprint& bp, RoomNode& room) {
    const i16v2& expandOffset = EXPAND_OFFSETS[enum_cast(wall.outerDir)];
    const i16v2& iterateOffset = ITERATE_OFFSETS[enum_cast(wall.outerDir)];

    wall.startPos += expandOffset;
    wall.endPos += expandOffset;
    extendWallEnd(*wall.startAdjacent, expandOffset);
    extendWallStart(*wall.endAdjacent, expandOffset);

    // Set new metadata
    i16v2 outerPos = wall.startPos;
    for (int j = 0; j < wall.length; ++j) {
        const ui16 index = getIndexAtPos(outerPos, bp.dims.x);
        RoomNodeID ownerId = bp.ownerArray[index];
        if (ownerId != INVALID_ROOM_ID) {
            RoomNode& ownerRoom = bp.nodes[ownerId];
            // TODO: Can we optimize this so we don't run it every time?
        }
        bp.ownerArray[index] = room.id;
        bp.tiles[index].type = BlueprintTileType::FLOOR;
        // Step
        outerPos += iterateOffset;
    }

    // TODO: Only along actual expansion tiles
    // TODO: Decrement size when we push into another room
    room.size += wall.length;
}

// Only fills gaps and will not overwrite any existing walls
void expandWallGapsOnly(RoomWall& wall, BuildingBlueprint& bp, RoomNode& room) {
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
        const ui16 index = getIndexAtPos(outerPos, bp.dims.x);
        RoomNodeID ownerId = bp.ownerArray[index];
        if (ownerId == INVALID_ROOM_ID) {
            ++sizeAdd;
            bp.ownerArray[index] = room.id;
            bp.tiles[index].type = BlueprintTileType::FLOOR;
        }
        // Step
        outerPos += iterateOffset;
    }

    // TODO: Only along actual expansion tiles
    // TODO: Decrement size when we push into another room
    room.size += wall.length;
}

bool expandRoomSquare(BuildingBlueprint& bp, RoomNode& room) {

   bool didExpand = false;

    for (int i = 0; i < room.numWalls; ++i) {

        f32 currentPressure = getPressureValue(room);
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
                RoomNodeID ownerId = bp.ownerArray[index];
                if (ownerId != INVALID_ROOM_ID) {
                    canExpand = false;
                    break;
                }
                // Step
                outerPos += iterateOffset;
            }
            // If we have room to expand, expand
            if (canExpand) {
                expandWall(wall, bp, room);
                didExpand = true;
            }
        }
    }
   return didExpand;
}

bool expandRoomGaps(BuildingBlueprint& bp, RoomNode& room) {

    bool didExpand = false;

    for (int iter = 0; iter < ITER_STEP; ++iter) {

        // Try expand walls
        int expandCount = 0;
        for (int i = 0; i < room.numWalls; ++i) {

            f32 currentPressure = getPressureValue(room);
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
                    RoomNodeID ownerId = bp.ownerArray[index];
                    if (ownerId == INVALID_ROOM_ID) {
                        canExpand = true;
                        break;
                    }
                    // Step
                    outerPos += iterateOffset;
                }
                // If we have room to expand, expand
                if (canExpand) {
                    expandWallGapsOnly(wall, bp, room);
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

void BuildingBlueprintGenerator::placeFacadeWalls(BuildingBlueprint& bp) const {
    for (RoomNodeID roomId = 0; roomId < bp.nodes.size(); ++roomId) {
        RoomNode& room = bp.nodes[roomId];
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
                if (bp.ownerArray[index] == roomId) {
                    i16v2 facadePos = pos + outerDir;
                    ui32 facadeIndex = facadePos.y * bp.dims.x + facadePos.x;
                    // Only place facade if this is an unowned tile
                    if (bp.ownerArray[facadeIndex] == INVALID_ROOM_ID) {
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
                if (bp.ownerArray[facadeIndex] == INVALID_ROOM_ID) {
                    bp.tiles[facadeIndex].type = BlueprintTileType::WALL;
                }
            }
        }
    }
}

void BuildingBlueprintGenerator::placeInteriorWalls(BuildingBlueprint& bp) const {
    // First place main segments
    for (ui16 y = 1; y < bp.dims.y - 1; ++y) {
        for (ui16 x = 1; x < bp.dims.x - 1; ++x) {
            ui16 index = y * bp.dims.x + x;
            RoomNodeID roomId = bp.ownerArray[index];
            if (roomId == INVALID_ROOM_ID) {
                continue;
            }
            int placedWalls = 0;
            { // Right
                ui16 nIndex = index + 1;
                RoomNodeID nId = bp.ownerArray[nIndex];
                if (nId != INVALID_ROOM_ID && nId != roomId) {
                    bp.tiles[nIndex].type = BlueprintTileType::WALL;
                    ++placedWalls;
                }
            }
            { // Down
                ui16 nIndex = index - bp.dims.x;
                RoomNodeID nId = bp.ownerArray[nIndex];
                if (nId != INVALID_ROOM_ID && nId != roomId) {
                    bp.tiles[nIndex].type = BlueprintTileType::WALL;
                    ++placedWalls;
                }
            }
        }
    }
    // Next place corners
    for (ui16 y = 1; y < bp.dims.y - 1; ++y) {
        for (ui16 x = 1; x < bp.dims.x - 1; ++x) {
            ui16 index = y * bp.dims.x + x;
            if (bp.tiles[index].type != BlueprintTileType::WALL) {
                continue;
            }
            // Down-right configuration
            {
                const ui16 downRightIndex = index - bp.dims.x + 1;
                if (bp.tiles[downRightIndex].type == BlueprintTileType::WALL) {
                    const ui16 downIndex = index - bp.dims.x;
                    if (bp.tiles[downIndex].type != BlueprintTileType::WALL) {
                        // Always place to right
                        bp.tiles[index + 1].type = BlueprintTileType::WALL;
                    }
                }
            }
            // Up-right configuration
            {
                const ui16 upRightIndex = index + bp.dims.x + 1;
                if (bp.tiles[upRightIndex].type == BlueprintTileType::WALL) {
                    const ui16 upIndex = index + bp.dims.x;
                    if (bp.tiles[upIndex].type != BlueprintTileType::WALL) {
                        // Always place to right
                        bp.tiles[index + 1].type = BlueprintTileType::WALL;
                    }
                }
            }
        }
    }
}

void BuildingBlueprintGenerator::expandRooms(BuildingBlueprint& bp) const {
    bp.tiles.resize((size_t)bp.dims.x * (size_t)bp.dims.y, { BlueprintTileType::NONE });
    bp.ownerArray.resize(bp.tiles.size(), INVALID_ROOM_ID);

    // Init rooms
    for (size_t i = 0; i < bp.nodes.size(); ++i) {
        initRoomWalls(bp, bp.nodes[i]);
    }

    // Expand walls in square shape, no overwrite
    for (int iters = 0; iters < MAX_WALL_LENGTH; ++iters) {
        int failCount = 0;
        for (size_t i = 0; i < bp.nodes.size(); ++i) {
            failCount += expandRoomSquare(bp, bp.nodes[i]) ? 0 : 1;
        }
        if (failCount == bp.nodes.size()) {
            break;
        }
    }
    // Fill in gaps, no overwrite
    for (int iters = 0; iters < MAX_WALL_LENGTH / ITER_STEP; ++iters) {
        int failCount = 0;
        for (size_t i = 0; i < bp.nodes.size(); ++i) {
            failCount += expandRoomGaps(bp, bp.nodes[i]) ? 0 : 1;
        }
        if (failCount == bp.nodes.size()) {
            break;
        }
    }
}

// Cellular Automata Sub-steps

ui8 ROOM_NODE_COUNT_CACHE[0xff + 0x1] = {};

// NOTE: ITeration order is important! We iterate to the right and then upwards
void FixupSingleRoomPieces(BuildingBlueprint& bp, ui16 x, ui16 y, ui16 index) {

    bool iterateAgain;

    do {
        Cartesian bestDir;
        iterateAgain = false;
        // Order doesnt matter here so go in cache-order
        const RoomNodeID myID = bp.ownerArray[index];
        if (myID == INVALID_ROOM_ID) {
            return;
        }

        ++ROOM_NODE_COUNT_CACHE[myID];
        // Down
        const RoomNodeID downID = bp.ownerArray[index - bp.dims.x];
        ++ROOM_NODE_COUNT_CACHE[downID];
        // Left
        const RoomNodeID leftID = bp.ownerArray[index - 1];
        ++ROOM_NODE_COUNT_CACHE[leftID];
        // Right
        const RoomNodeID rightID = bp.ownerArray[index + 1];
        ++ROOM_NODE_COUNT_CACHE[rightID];
        // Top
        const RoomNodeID topID = bp.ownerArray[index + bp.dims.x];
        ++ROOM_NODE_COUNT_CACHE[topID];

        // If we are surrounded on 3 or more sides
        const ui8 myCount = ROOM_NODE_COUNT_CACHE[myID];
        ROOM_NODE_COUNT_CACHE[myID] = 0;
        if (myCount <= 2) {
            RoomNodeID bestId = myID;
            ui8 bestCount = 0;

            // Down
            const ui8 downCount = ROOM_NODE_COUNT_CACHE[downID];
            if (downCount > bestCount) {
                bestId = downID;
                bestCount = downCount;
                iterateAgain = true;
                bestDir = Cartesian::DOWN;
            }
            // Left
            const ui8 leftCount = ROOM_NODE_COUNT_CACHE[leftID];
            if (leftCount > bestCount) {
                bestId = leftID;
                bestCount = leftCount;
                iterateAgain = true;
                bestDir = Cartesian::LEFT;
            }

            // Right
            const ui8 rightCount = ROOM_NODE_COUNT_CACHE[rightID];
            if (rightCount > bestCount) {
                bestId = rightID;
                bestCount = rightCount;
                iterateAgain = true;
                bestDir = Cartesian::RIGHT;
            }

            // Top
            const ui8 topCount = ROOM_NODE_COUNT_CACHE[topID];
            if (topCount > bestCount) {
                bestId = topID;
                bestCount = topCount;
                iterateAgain = true;
                bestDir = Cartesian::UP;
            }

            // Replace!
            if (bestId != myID) {
                if (bestId != INVALID_ROOM_ID) {
                    ++bp.nodes[bestId].size;
                }

                bp.ownerArray[index] = bestId;
                --bp.nodes[myID].size;
            }
            else if (myID != INVALID_ROOM_ID) {
                bp.ownerArray[index] = INVALID_ROOM_ID;
                --bp.nodes[myID].size;
            }
        }


        // Clear cache
        ROOM_NODE_COUNT_CACHE[downID] = 0;
        ROOM_NODE_COUNT_CACHE[leftID] = 0;
        ROOM_NODE_COUNT_CACHE[myID] = 0;
        ROOM_NODE_COUNT_CACHE[rightID] = 0;
        ROOM_NODE_COUNT_CACHE[topID] = 0;

        if (iterateAgain) {
            // TODO: CONSTANTS
            // Shift to next tile
            switch (bestDir) {
                case Cartesian::DOWN: // down
                    if (y == 1) {
                        return;
                    }
                    --y;
                    index -= bp.dims.x;
                    break;
                case Cartesian::LEFT: // left
                    if (x == 1) {
                        return;
                    }
                    --x;
                    --index;
                    break;
                case Cartesian::RIGHT: // right
                    if (x == bp.dims.x - 2) {
                        return;
                    }
                    ++x;
                    ++index;
                    break;
                case Cartesian::UP: // up
                    if (y == bp.dims.y - 2) {
                        return;
                    }
                    ++y;
                    index += bp.dims.x;
                    break;
            }
        }
        else {
            return;
        }
    } while (true);
}

// Sweeping Cellular Automata
void CellularAutomataSubStepThickenPassages(BuildingBlueprint& bp, ui16 x, ui16 y, ui16 index, RoomNode& room) {

    // Order matters here, we want to thicken ahead of us and create a wave.
    // With this order, we are allowed to modify Right, up, and ourselves, but not down or left.

    // When expanding, use a pressure function.
    const f32 pressure = getPressureValue(room); // Outward - If > 1, then we are trying to grow. If < 1, then we are trying to shrink
   

    // CAN ONLY MODIFY OURSELVES
    // Right
    //if (x < bp.dims.x) {
    //    RoomNodeID& rightTile = bp.ownerArray[index + 1];
    //    f32 currentPressure;
    //    // Always expand into invalid
    //    if (rightTile != INVALID_ROOM_ID) {
    //        RoomNode& rightRoom = bp.nodes[rightTile];
    //        const f32 neighborPressure = getPressureValue(rightRoom);
    //        f32 pressureRatio = pressure / neighborPressure; // Outward - If > 1, then we are trying to grow. If < 1, then we are trying to shrink
    //        f32 sizePressure = rightRoom.size / room.size; // Outward - If > 1, then we are trying to grow. If < 1, then we are trying to shrink
    //        currentPressure = sizePressure * pressureRatio;

    //        if (currentPressure > 1) {
    //            --rightRoom.size;
    //            ++room.size;
    //            rightTile = room.id;
    //        }
    //    }
    //}
    // Up
    // Down
    // Left
}

void BuildingBlueprintGenerator::roomCleanup(BuildingBlueprint& bp) const
{
    constexpr int CELLULAR_AUTOMATA_ITERATIONS = 1;

    for (int step = 0; step < CELLULAR_AUTOMATA_ITERATIONS; ++step) {
        for (ui16 y = 1; y < bp.dims.y - 1; ++y) {
            for (ui16 x = 1; x < bp.dims.x - 1; ++x) {
                ui16 index = y * bp.dims.x + x;
                FixupSingleRoomPieces(bp, x, y, index);
            }
        }
    }
}

void BuildingBlueprintGenerator::initRoomWalls(BuildingBlueprint& bp, RoomNode& room) const
{
    ui16 index = getIndexAtPos(room.offsetFromZero, bp.dims.x);
    // Init root node
    room.size = 1;
    bp.tiles[index].type = BlueprintTileType::FLOOR;
    bp.ownerArray[index] = room.id;

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

struct DoorBFSNode {
    ui32 index;
};

void doorBfs(std::vector<DoorBFSNode>& bfs, size_t& bfsBackIndex, BuildingBlueprint& bp, RoomWallOuterDir dir, ui32 nodeIndex, RoomNode& room, const i16v2& currentPos, std::vector<bool>& visited, std::vector<bool>& isConnected, bool& canConnectToOutside) {
    const i16v2& directionOffset = EXPAND_OFFSETS[enum_cast(dir)];
    const i16v2 nextPos = currentPos + directionOffset;
    i16 nextIndex = getIndexAtPos(nextPos, bp.dims.x);
    if (!visited[nextIndex]) {
        RoomNodeID nextId = bp.ownerArray[nextIndex];
        visited[nextIndex] = true;
        if (nextId == bp.ownerArray[nodeIndex]) {

            bfs[bfsBackIndex++].index = nextIndex;
            if (bfsBackIndex >= bfs.size()) bfsBackIndex = 0;
        }
        else {

            if (nextId == INVALID_ROOM_ID) {

                if (canConnectToOutside) {

                    canConnectToOutside = false;
                    if (bp.tiles[nodeIndex].type == BlueprintTileType::WALL && bp.tiles[nextIndex].type == BlueprintTileType::FLOOR) {
                        bp.tiles[nodeIndex].type = BlueprintTileType::DOOR;
                    }
                    if (bp.tiles[nextIndex].type == BlueprintTileType::WALL && bp.tiles[nodeIndex].type == BlueprintTileType::FLOOR) {

                        bp.tiles[nextIndex].type = BlueprintTileType::DOOR;
                    }
                }
            }
            else if (!isConnected[nextId] && room.numAdjacentRooms < MAX_ADJACENT_ROOMS) {

                RoomNode& adjacent = bp.nodes[nextId];
                if (adjacent.numAdjacentRooms < MAX_ADJACENT_ROOMS) {

                    if (bp.tiles[nextIndex].type == BlueprintTileType::WALL && bp.tiles[nodeIndex].type == BlueprintTileType::FLOOR) {

                        const i16v2 outerPos = nextPos + directionOffset;
                        // Bounds check
                        switch (dir) {
                            case RoomWallOuterDir::LEFT:
                                if (outerPos.x < 0) return;
                                break;
                            case RoomWallOuterDir::BOTTOM:
                                if (outerPos.y < 0) return;
                                break;
                            case RoomWallOuterDir::RIGHT:
                                if (outerPos.x >= bp.dims.x) return;
                                break;
                            case RoomWallOuterDir::TOP:
                                if (outerPos.y >= bp.dims.y) return;
                                break;
                        }

                        i16 outerIndex = getIndexAtPos(outerPos, bp.dims.x);
                        if (bp.tiles[outerIndex].type <= BlueprintTileType::FLOOR) {
                            isConnected[nextId] = true;
                            room.adjacentRooms[room.numAdjacentRooms++] = nextId;
                            adjacent.adjacentRooms[adjacent.numAdjacentRooms++] = bp.ownerArray[nodeIndex];
                            bp.tiles[nextIndex].type = BlueprintTileType::DOOR;
                        }
                    }
                }
            }
        }
    }
}

void BuildingBlueprintGenerator::placeDoors(BuildingBlueprint& bp) const {
    // TODO: Re-use memory
    std::vector<bool> visited((ui32)bp.dims.x * (ui32)bp.dims.y);
    std::vector<bool> isConnected(bp.nodes.size());

    // Ringbuffer
    // TODO: Re-use memory
    std::vector<DoorBFSNode> bfs((ui32)bp.dims.x * (ui32)bp.dims.y);
    size_t bfsFrontIndex;
    size_t bfsBackIndex;
    bool canConnectToOutside = true;
    for (auto&& room : bp.nodes) {
        // Clear visited list
        std::fill(visited.begin(), visited.end(), 0);
        std::fill(isConnected.begin(), isConnected.end(), 0);

        // Mark neighbor rooms as connected
        for (int i = 0; i < room.numAdjacentRooms; ++i) {
            isConnected[room.adjacentRooms[i]] = true;
        }

        bfsFrontIndex = 0;
        bfsBackIndex = 1;

        ui32 startIndex = getIndexAtPos(room.offsetFromZero, bp.dims.x);
        visited[startIndex] = true;
        bfs[bfsFrontIndex].index = startIndex;
        // Do the bfs
        while (bfsFrontIndex != bfsBackIndex) {
            const DoorBFSNode& node = bfs[bfsFrontIndex];
            i32v2 pos(node.index % bp.dims.x, node.index / bp.dims.x);
            RoomNodeID roomId = bp.ownerArray[node.index];
            // Left
            if (pos.x > 0) {
                doorBfs(bfs, bfsBackIndex, bp, RoomWallOuterDir::LEFT, node.index, room, pos, visited, isConnected, canConnectToOutside);
            }

            // Bottom
            if (pos.y > 0) {
                doorBfs(bfs, bfsBackIndex, bp, RoomWallOuterDir::BOTTOM, node.index, room, pos, visited, isConnected, canConnectToOutside);
            }

            // Right
            if (pos.x < bp.dims.x - 1) {
                doorBfs(bfs, bfsBackIndex, bp, RoomWallOuterDir::RIGHT, node.index, room, pos, visited, isConnected, canConnectToOutside);
            }

            // Up
            if (pos.y < bp.dims.y - 1) {
                doorBfs(bfs, bfsBackIndex, bp, RoomWallOuterDir::TOP, node.index, room, pos, visited, isConnected, canConnectToOutside);
            }
            ++bfsFrontIndex;
        }
    }
}
