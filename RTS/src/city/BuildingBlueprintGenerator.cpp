#include "stdafx.h"
#include "BuildingBlueprintGenerator.h"
#include "BuildingRepository.h"
#include "BuildingBlueprintGenerationContext.h"

#include "gamethread/GameThreadTasks.h"

#include "city/CityBuilder.h"

#include "resources/TileRepository.h"

#include <Vorb/Timing.h>
#include "math/Random.h"

#include "debugging/VisualLogger.h"
#include "util/GridEdgeFinder.h"

#include <boost/container/static_vector.hpp>

// For vislog
constexpr int MAX_ROOM_COLORS = 8;
constexpr float ROOM_COLOR_ALPHA = 0.6f;
constexpr float ROOM_COLOR_ALPHA_WHITE = 0.7f;
const color4 ROOM_COLORS[MAX_ROOM_COLORS] = {
    color4(1.0f, 0.5f, 0.0f, ROOM_COLOR_ALPHA),
    color4(0.0f, 1.0f, 0.5f, ROOM_COLOR_ALPHA),
    color4(0.5f, 0.0f, 1.0f, ROOM_COLOR_ALPHA),
    color4(1.0f, 1.0f, 0.0f, ROOM_COLOR_ALPHA),
    color4(0.0f, 1.0f, 1.0f, ROOM_COLOR_ALPHA),
    color4(1.0f, 0.7f, 0.7f, ROOM_COLOR_ALPHA_WHITE),
    color4(1.0f, 0.0f, 1.0f, ROOM_COLOR_ALPHA),
    color4(0.5f, 0.5f, 0.5f, ROOM_COLOR_ALPHA),
};

void renderBlueprintDebugVislog(BuildingBlueprintGenerationContext& context, VisualLog& visLog, color4* inputColor/* = nullptr*/) {
    visLog.nextStep("Final Floorplan");
    // Render the AABB of the floor plan
    constexpr f32 EPSILON = 0.001f;

    // Entire AABB
    const i32AABB3& aabb = context.mTileSpatialGrid.getAABB();
    const i32 floorHeight = context.mTileSpatialGrid.getFloorHeight();
    visLog.addWireQuad(f32v3(0.0f, 0.0f, 0.0f), aabb.dims, inputColor ? *inputColor : color4(0.7f, 0.4f, 0.0f));

    // AABBS first
    int i = 0;
    for (auto&& node : context.rooms) {

        const color4& color = ROOM_COLORS[i % MAX_ROOM_COLORS];
        visLog.addWireQuad(f32v3(node.aabb.x - aabb.x, node.aabb.y - aabb.y, node.floorIndex * floorHeight), node.aabb.dims, color4(color.r, color.g, color.b, 255u));
        // Draw parent line
        if (node.parentRoom != INVALID_ROOM_ID) {
            RoomNode& parent = context.rooms[node.parentRoom];
            const f32v3 startPos(node.offsetFromZero.x + 0.5f, node.offsetFromZero.y + 0.5f, node.floorIndex * floorHeight);
            const f32v3 endPos(parent.offsetFromZero.x + 0.5f, parent.offsetFromZero.y + 0.5f, parent.floorIndex * floorHeight);
            if (node.isPrivate) {
                visLog.addLineBetweenPoints(startPos, endPos, color4(1.0f, 1.0f, 0.0f));
            }
            else {
                visLog.addLineBetweenPoints(startPos, endPos, color4(0.0f, 1.0f, 0.0f));
            }
        }

        // Room name
        char buf[64];
        node.roomDef->getName().toString(buf, nullptr);
        visLog.addText(buf, f32v3(node.offsetFromZero.x + 0.5f, node.offsetFromZero.y + 0.5f, node.floorIndex * floorHeight), 0.25f, f32v2(0.0f, 0.5f), color4(color.r, color.g, color.b, 255u));
        ++i;
    }

    // Render all the tiles
    for (int z = 0; z < context.floorCount; ++z) {
        const i32 floorIndex = z * aabb.dims.x * aabb.dims.y;
        for (int y = 0; y < aabb.dims.y; ++y) {
            for (int x = 0; x < aabb.dims.x; ++x) {
                const TileIndex index = floorIndex + y * aabb.dims.x + x;
                RoomNodeID id = context.ownerArray[index];
                if (id != INVALID_ROOM_ID) {
                    const RoomNode& room = context.rooms[id];
                    const f32v2 pos = f32v2(x, y);
                    const color4& color = inputColor ? *inputColor : ROOM_COLORS[id % MAX_ROOM_COLORS];
                    visLog.addFilledQuad(f32v3(pos.x, pos.y, room.floorIndex * floorHeight), f32v2(1.0f), color4(color.r, color.g, color.b, 128u));
                }
            }
        }
    }

}

inline bool boundsCheckTile(const i16v2& pos, const i32AABB2& aabb) {
    return pos.x >= 0 && pos.x < aabb.width&& pos.y >= 0 && pos.y < aabb.depth;
}

// Dont place on outer border, thats where facade goes
bool boundsCheckRoom(i16 pos, i16 dim) {
    return pos >= 1 && pos < dim - 1;
}

struct WallInfo {
    i32v2 startPos;
    i32 length;
};


WallInfo getWallInfoFromRoomAABB(Cartesian wallDir, const i32AABB2& aabb) {
    WallInfo rv;
    switch (wallDir) {
        case Cartesian::SOUTH:
            rv.startPos = aabb.pos;
            rv.length = aabb.width;
            break;
        case Cartesian::WEST:
            rv.startPos = aabb.pos;
            rv.length = aabb.depth;
            break;
        case Cartesian::EAST:
            rv.startPos = aabb.pos;
            rv.startPos.x += aabb.width - 1;
            rv.length = aabb.depth;
            break;
        case Cartesian::NORTH:
            rv.startPos = aabb.pos;
            rv.startPos.y += aabb.depth - 1;
            rv.length = aabb.width;
            break;
        default:
            assert(false);
            break;

    }
    return rv;
}


RUNTIME_INIT_FUNC(generateWindowPermutations) {
    BuildingBlueprintGenerator::generatePossibleWindowPermutations();
}


BuildingBlueprintPtr BuildingBlueprintGenerator::tryGenerateBlueprintSynchronous(
    const BuildingDef& desc,
    float sizeAlpha,
    Cartesian entrySide,
    DTileCoord worldPosRoot,
    i32v2 plotSizeDTiles,
    const BitArray& ownedDTiles,
    BuildingBlueprintFlags flags,
    ui32 seed
) {

    PROFILE_FUNCTION();
    constexpr ui32 maxFailCount = 5;
    ui32 failCount = 0;
    // Get new seed each fail
    RandomGenerator seedMutator(seed);

    do {
        BuildingBlueprintGenerationContext context(desc, sizeAlpha, entrySide, plotSizeDTiles, worldPosRoot, flags);
        context.generationSeed = seed;
        context.randomGen = std::make_unique<RandomGenerator>(seed);
        assert(plotSizeDTiles.x > 2 && plotSizeDTiles.y > 2);
        if (tryGenerateBlueprintInternal(context)) {
            if (failCount > 0) {
                LOG_DEBUG("Finished building with fail count {}", failCount);
            }
            return finalizeBlueprint(context);
        }
        seed = seedMutator.getRandomUint();
    } while (++failCount < maxFailCount);

    LOG_WARN("Failed to generate building with fail count {}", failCount);
    return nullptr;
}

void BuildingBlueprintGenerator::generatePossibleWindowPermutations() {
    // TODO: Algorithmic
    sPossibleWindowPermutations[0] = {};
    sPossibleWindowPermutations[1] = {
        {0},
    };
    sPossibleWindowPermutations[2] = {
        {0, 0},
    };
    sPossibleWindowPermutations[3] = {
        {0, 0, 0}, {0, 1, 0}
    };
    sPossibleWindowPermutations[4] = {
        {0, 0, 0, 0}, {0, 1, 1, 0},
    };
    sPossibleWindowPermutations[5] = {
        {0, 0, 0, 0, 0}, {0, 0, 1, 0, 0}, {0, 1, 1, 0, 0}, {0, 0, 1, 1, 0}
    };
    sPossibleWindowPermutations[6] = {
        {0, 0, 0, 0, 0, 0}, {0, 0, 1, 1, 0, 0}, {0, 1, 1, 0, 0, 0}, {0, 0, 0, 1, 1, 0}
    };
    sPossibleWindowPermutations[7] = {
        {0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 1, 0, 0, 0}, {0, 0, 1, 1, 1, 0, 0}, {0, 1, 1, 0, 1, 1, 0},
    };
    sPossibleWindowPermutations[8] = {
        {0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 1, 1, 0, 0, 0}, {0, 1, 1, 0, 0, 1, 1, 0},
    };

    static_assert(MAX_EXTERIOR_WALL_RUN_LENGTH == 8);
}

bool BuildingBlueprintGenerator::tryGenerateBlueprintInternal(BuildingBlueprintGenerationContext& context) {

    
    VisualLog* visLog = VisualLogger::tryGetNewVisualLog("Blueprint - Seed: " + std::to_string(context.randomGen->mSeed), VisualLogCategory::Building, false);
    if (visLog) {
        visLog->setRootPos(f32v3(context.mTileSpatialGrid.getWorldPos3D()));
        const i32v3 dims = context.mTileSpatialGrid.getDims();
        visLog->setUserString(fmt::format("  Dims <{},{},{}>", dims.x, dims.y, dims.z));
    }

    // Room Graph
    addPublicRoomsToGraph(context);
    assignPublicRooms(context);
    addPrivateRoomsToGraph(context);

    // Assign roomDefs
    RoomRepository& roomRepo = RoomRepository::get();
    for (auto& room : context.rooms) {
        room.roomDef = &roomRepo.getLoadedOrUnloadedAsset(room.roomDefId);
    }

    // Rooms
    initRooms(context);
    placeRooms(context, visLog);
    allocateTileData(context);
    expandRooms(context, visLog);
    roomCleanup(context, visLog);
    computeRoomAABBs(context, visLog);
    if (!validateRoomsArentEmpty(context, visLog)) {
        if (visLog) {
            visLog->nextStep("INVALID GENERATION - EMPTY ROOM");
            visLog->finish();
        }
        return false;
    }

    // Walls
    placeWalls(context, visLog);

    // Room edge info
    buildRoomInteriorEdges(context, visLog);

    // Doors
    placeDoors(context, visLog);

    // Stairs
    placeStairs(context, visLog);

    // Windows + facade details
    buildExteriorWallRuns(context, visLog);
    placeWindows(context, visLog);

    // Furniture

    // Flooring

    // Tally final item requirements
    postProcessBlueprint(context);

    if (visLog) {
        // Draw the entire room graph
        renderBlueprintDebugVislog(context, *visLog, nullptr);
        visLog->finish();
    }

    return true;
}

void BuildingBlueprintGenerator::addPublicRoomsToGraph(BuildingBlueprintGenerationContext& context) {
    
    // Generate public room structure using grammar
    const i32 publicRoomCount = context.desc->publicRoomCountRange.y <= context.desc->publicRoomCountRange.x ?
        context.desc->publicRoomCountRange.x : context.randomGen->getRandomUint() % (context.desc->publicRoomCountRange.y - context.desc->publicRoomCountRange.x) + context.desc->publicRoomCountRange.x;
    assert(publicRoomCount); // Must have at least one public room
    context.rooms.resize(publicRoomCount);
    context.desc->publicGrammar.buildRoomGraph(context.rooms, *context.randomGen);
}

void BuildingBlueprintGenerator::assignPublicRooms(BuildingBlueprintGenerationContext& context)
{
    
    assert(context.desc->publicRooms.size());
    ui8v2 countLookup[255]; // (current, max)
    const i32 publicRoomCount = (i32)context.desc->publicRooms.size();
    i32 availablePublicRooms = 0;

    { // Pre-pass set up count lookup
        i32 roomIndex = 0;
        for (roomIndex = 0; roomIndex < publicRoomCount; ++roomIndex) {
            const PossibleRoom& room = context.desc->publicRooms[roomIndex];
            countLookup[roomIndex].x = 0;
            countLookup[roomIndex].y = room.countRange.y;
            availablePublicRooms += room.countRange.y;
        }
    }
    assert(availablePublicRooms >= publicRoomCount);

    {// Generate rooms in order of priority while breadth first walking the tree
        i32 roomIndex = 0;
        for (auto&& node : context.rooms) {
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
            --availablePublicRooms;
            node.roomDefId = context.desc->publicRooms[roomIndex++].id;
            
            if (availablePublicRooms == 0) {
                break;
            }
            // Wrap
            if (roomIndex >= publicRoomCount) {
                roomIndex = 0;
            }
        }
    }
}

void BuildingBlueprintGenerator::addPrivateRoomsToGraph(BuildingBlueprintGenerationContext& context) {
    
    const size_t numPublicRooms = context.rooms.size();
    ui8v2 countLookup[255]; // (current, max)
    assert(numPublicRooms);
    i32 privateRoomCount = round(context.sizeAlpha * (context.desc->privateRoomCountRange.y - context.desc->privateRoomCountRange.x) + context.desc->privateRoomCountRange.x);
    if (!privateRoomCount) {
        return;
    }

    i32 availablePrivateRooms = 0;

    { // Pre-pass set up count lookup
        i32 roomIndex = 0;
        for (roomIndex = 0; roomIndex < context.desc->privateRooms.size(); ++roomIndex) {
            const PossibleRoom& room = context.desc->privateRooms[roomIndex];
            ui8 roomCount = vmath::roundApprox(vmath::lerp((f32)room.countRange.x, (f32)room.countRange.y, context.sizeAlpha));
            countLookup[roomIndex].x = 0;
            countLookup[roomIndex].y = roomCount;
            availablePrivateRooms += roomCount;
        }
    }
    if (availablePrivateRooms < privateRoomCount) {
        privateRoomCount = availablePrivateRooms;
    }

    context.rooms.reserve(context.rooms.size() + privateRoomCount);
    int failCount = 0;
    int publicIndex = context.randomGen->getRandomUint() % numPublicRooms; // Random start room to test
    int privateIndex = 0;
    for (size_t i = 0; i < privateRoomCount; ++i) {
        RoomNode& publicRoom = context.rooms[publicIndex];
        if (publicRoom.numChildren < MAX_CHILD_ROOMS && countLookup[privateIndex].x < countLookup[privateIndex].y) {
            // We can fit a private room here
            // Next node index is our child
            publicRoom.childRooms[publicRoom.numChildren++] = (RoomNodeID)context.rooms.size();
            // Append the room
            RoomNode privateRoom;
            privateRoom.roomDefId = context.desc->privateRooms[privateIndex].id;
            privateRoom.parentRoom = publicIndex;
            privateRoom.isPrivate = true;
            context.rooms.emplace_back(std::move(privateRoom));
            // Limit our private count
            ++countLookup[privateIndex++].x;
            // Wrap
            if (privateIndex >= context.desc->privateRooms.size()) {
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

i32 getMaximumDepthRecursive(std::vector<RoomNode>& nodes, RoomNode* node) {
    i32 maximumChildDepth = 0;
    for (int i = 0; i < node->numChildren; ++i) {
        const i32 depth = getMaximumDepthRecursive(nodes, &nodes[node->childRooms[i]]);
        if (depth > maximumChildDepth) {
            maximumChildDepth = depth;
        }
    }
    return maximumChildDepth + 1;
}

void placeChildrenRecursive(BuildingBlueprintGenerationContext& context, RoomNode* node, f32 availableWidthSpan, i32 maxXOffsetPerLayer, i32v2 currentOffset, const i32v2& dims2d, VisualLog* visLog) {
    if (node->numChildren == 0) {
        return;
    }
    
    assert(currentOffset.x < 10000 && currentOffset.y < 10000);
    std::vector<RoomNode>& nodes = context.rooms;
    const f32 floorHeight = context.mTileSpatialGrid.getFloorHeight();

    // TODO: Worry about even vs odd?
    const i32 myDesiredRadius = node->desiredWidth / 2;

    // Determine desired child y span
    i32 totalChildSpan = 0;
    for (int i = 0; i < node->numChildren; ++i) {
        RoomNode& child = nodes[node->childRooms[i]];
        totalChildSpan += child.desiredWidth;
        // Initialize child floor to our floor
        child.floorIndex = node->floorIndex;

    }

    const f32 desiredWidthSpan = vmath::min((f32)totalChildSpan, availableWidthSpan);
    
    // Place a child above with stairs if possible
    bool didCreateStairs = false;
    for (int i = 0; i < node->numChildren; ++i) {
        RoomNode& child = nodes[node->childRooms[i]];
        // New floor TODO: Random? Statistics?
        // Never two rooms with stairs stacked on each other.
        if (node->roomDef->stairsChance && !node->connectedToParentWithStairs && child.roomDef->canStairsConnect && child.desiredWidth >= 3) {
            if (context.randomGen->getRandomFloatUnsigned() > node->roomDef->stairsChance) {
                didCreateStairs = true;
                node->hasStairs = true;

                child.connectedToParentWithStairs = true;
                child.offsetFromZero = currentOffset;
                child.floorIndex = node->floorIndex + 1;
                // Force size to match
                child.desiredWidth = node->desiredWidth;
                child.desiredSize = node->desiredSize;
                if (child.floorIndex == context.floorCount) {
                    ++context.floorCount;
                }
                // Visual log
                if (visLog) {
                    const color4& color = ROOM_COLORS[node->childRooms[i] % MAX_ROOM_COLORS];
                    const f32v3 childPos(child.offsetFromZero.x, child.offsetFromZero.y, child.floorIndex * floorHeight);
                    visLog->addWireQuad(childPos, f32v2(1.0f), color);
                    const f32v3 parentPos(node->offsetFromZero.x, node->offsetFromZero.y, node->floorIndex * floorHeight);
                    visLog->addLineBetweenPoints(childPos, parentPos, color);

                    char buf[64];
                    child.roomDef->getName().toString(buf, nullptr);
                    visLog->addText(buf, f32v3(childPos.x + 0.5f, childPos.y + 0.5f, child.floorIndex * floorHeight), 0.25f, f32v2(0.0f, 0.5f), color4(color.r, color.g, color.b, 255u));
                }
                placeChildrenRecursive(context, &child, availableWidthSpan, maxXOffsetPerLayer, child.offsetFromZero, dims2d, visLog);
                break;
            }
        }
    }

    const ui8 numChildrenOnSameFloor = node->numChildren - (int)didCreateStairs;

    // TODO: Dynamic child Y span based on available space?
    // Place any children on same floor
    if (numChildrenOnSameFloor) {
        f32 childWidthSpan = desiredWidthSpan / numChildrenOnSameFloor;
        f32 widthSegmentSize = childWidthSpan / 2.0f;
        // Start at the top
        currentOffset.y -= (i32)(widthSegmentSize * (node->numChildren - 1));
        for (int i = 0; i < node->numChildren; ++i) {
            RoomNode& child = nodes[node->childRooms[i]];
            // If above, continue since we did this already
            if (child.floorIndex == node->floorIndex + 1) {
                continue;
            }

            const i32 childDesiredRadius = child.desiredWidth / 2;

            i32 xOffset = vmath::min(maxXOffsetPerLayer, (i32)(myDesiredRadius + childDesiredRadius));
            if (xOffset < 1) xOffset = 1;

            child.offsetFromZero = currentOffset;
            child.offsetFromZero.x += xOffset;
            // Visual log
            if (visLog) {
                const color4& color = ROOM_COLORS[node->childRooms[i] % MAX_ROOM_COLORS];
                const f32v3 childPos(child.offsetFromZero.x, child.offsetFromZero.y, child.floorIndex * floorHeight);
                visLog->addWireQuad(childPos, f32v2(1.0f), color);
                const f32v3 parentPos(node->offsetFromZero.x, node->offsetFromZero.y, node->floorIndex * floorHeight);
                visLog->addLineBetweenPoints(childPos, parentPos, color);
                char buf[64];
                child.roomDef->getName().toString(buf, nullptr);
                visLog->addText(buf, f32v3(childPos.x + 0.5f, childPos.y + 0.5f, child.floorIndex * floorHeight), 0.25f, f32v2(0.0f, 0.5f), color4(color.r, color.g, color.b, 255u));
            }
            assert(child.offsetFromZero.x < 10000 && child.offsetFromZero.y < 10000);
            placeChildrenRecursive(context, &child, childWidthSpan, maxXOffsetPerLayer, child.offsetFromZero, dims2d, visLog);
            currentOffset.y += widthSegmentSize * 2;
        }
    }
}

void BuildingBlueprintGenerator::initRooms(BuildingBlueprintGenerationContext& context) {
    
    RoomRepository& roomRepo = RoomRepository::get();
    for (size_t i = 0; i < context.rooms.size(); ++i) {
        RoomNode& room = context.rooms[i];
        room.id = (RoomNodeID)i;

        const RoomDef& desc = roomRepo.getLoadedOrUnloadedAsset(room.roomDefId);
        room.desiredWidth = (i32)round(lerp((f32)desc.widthRange.x, (f32)desc.widthRange.y, context.sizeAlpha));
        room.desiredSize = room.desiredWidth * room.desiredWidth; //SQ
    }
}

void applyForceOffset(ui16v2& offset, const f32v2& force, const i32v2& dims) {
    assert(offset.x < 10000 && offset.y < 10000);
    i32v2 newOffset = i32v2(offset) + i32v2(force);
    offset.x = vmath::clamp(newOffset.x, 1, dims.x - 2);
    offset.y = vmath::clamp(newOffset.y, 1, dims.y - 2);
    assert(offset.x < 10000 && offset.y < 10000);
}

void BuildingBlueprintGenerator::placeRooms(BuildingBlueprintGenerationContext& context, VisualLog* visLog) {
    
    if (visLog) visLog->nextStep("Place rooms");
    // Breadth first search room placement
    RoomNode* root = &context.rooms[0];
    i32 maximumDepth = getMaximumDepthRecursive(context.rooms, root);
    const i32v3& dims3D = context.mTileSpatialGrid.getDims();

    // Determine which dims to use for cartesian
    i32v2 dims;
    switch (context.entrySide) {
        case Cartesian::SOUTH:
        case Cartesian::NORTH:
            dims.x = dims3D.y;
            dims.y = dims3D.x;
            break;
        case Cartesian::WEST:
        case Cartesian::EAST:
            dims.x = dims3D.x;
            dims.y = dims3D.y;
            break;
    }
    const i32 maxDepthOffsetPerLayer = dims.x / maximumDepth;
    assert(maximumDepth < dims.x);

    const f32 availableWidthSpan = (f32)dims.y;
    // Place the root, +1 so we are less likely to touch the side of the AABB
    root->offsetFromZero = i32v2(vmath::min(maxDepthOffsetPerLayer / 2, (i32)root->desiredWidth / 2) + 1, dims.y / 2);
    if (root->offsetFromZero.x == 0) root->offsetFromZero.x = 1u;
    assert(root->offsetFromZero.x < 10000 && root->offsetFromZero.y < 10000);

    // We will generate to the right, then will rotate the coordinates around based on the cartesian
    if (visLog) {
        char buf[64];
        root->roomDef->getName().toString(buf, nullptr);
        visLog->addText(buf, f32v3(root->offsetFromZero.x + 0.5f, root->offsetFromZero.y + 0.5f, root->floorIndex * context.mTileSpatialGrid.getFloorHeight()), 0.25f, f32v2(0.0f, 0.5f), COLOR_WHITE);
    }
    placeChildrenRecursive(context, root, availableWidthSpan, maxDepthOffsetPerLayer, root->offsetFromZero, dims, visLog);

    // Rotate all coordinates around for Cartesian direction
    // Left is the base case so do nothing for that
    switch (context.entrySide) {
        case Cartesian::SOUTH:
            for (auto&& room : context.rooms) {
                i32 tmp = room.offsetFromZero.x;
                room.offsetFromZero.x = room.offsetFromZero.y;
                room.offsetFromZero.y = dims3D.y - tmp - 1;
            }
            break;
        case Cartesian::EAST:
            for (auto&& room : context.rooms) {
                room.offsetFromZero.x = dims3D.x - room.offsetFromZero.x - 1;
            }
            break;
        case Cartesian::NORTH:
            for (auto&& room : context.rooms) {
                std::swap(room.offsetFromZero.x, room.offsetFromZero.y);
                room.offsetFromZero.x = dims3D.x - room.offsetFromZero.x - 1;
            }
            break;
    }

    // Clamp positions to be within outer facade
    for (auto&& room : context.rooms) {
        room.offsetFromZero.x = vmath::clamp((i32)room.offsetFromZero.x, (i32)1u, (i32)dims3D.x - 1);
        room.offsetFromZero.y = vmath::clamp((i32)room.offsetFromZero.y, (i32)1u, (i32)dims3D.y - 1);
        assert(room.offsetFromZero.x < 10000 && room.offsetFromZero.y < 10000);
    }

    // Push away from unowned tiles by selecting closest owned tile
    for (auto&& room : context.rooms) {
        if (!context.isLocalTileIndexOwned(room.offsetFromZero)) {
            assert(false);
            X;
            // TODO: Cache all owned local tiles sorted so we can not only quickly test if owned, we can also
            // find the nearest owned tile.
        }
    }


    // Spread rooms apart based on circular collision
    constexpr f32 FORCE_MULT = 0.5f;
    for (int iter = 0; iter < 3; ++iter) {
        for (size_t i = 0; i < context.rooms.size() - 1; ++i) {
            RoomNode& room1 = context.rooms[i];
            for (size_t j = i + 1; j < context.rooms.size(); ++j) {
                RoomNode& room2 = context.rooms[j];
                if (room2.floorIndex != room1.floorIndex) {
                    // Not influenced by rooms on other floors
                    continue;
                }

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
                    applyForceOffset(room1.offsetFromZero, -pushForce, dims3D);
                    applyForceOffset(room2.offsetFromZero, pushForce, dims3D);
                } else if (room2.parentRoom == i) { // Magnet only to children
                    const f32v2 pullForce = offset * ((distance - desiredDistance) * FORCE_MULT);
                    applyForceOffset(room1.offsetFromZero, pullForce, dims3D);
                    applyForceOffset(room2.offsetFromZero, -pullForce, dims3D);
                }
            }
        }
    }

    // Second spread pass aiming only at tiny distances
    for (int iter = 0; iter < 2; ++iter) {
        for (size_t i = 0; i < context.rooms.size() - 1; ++i) {
            RoomNode& room1 = context.rooms[i];
            for (size_t j = i + 1; j < context.rooms.size(); ++j) {
                RoomNode& room2 = context.rooms[j];
                if (room2.floorIndex != room1.floorIndex) {
                    // Not influenced by rooms on other floors
                    continue;
                }

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
                    applyForceOffset(room1.offsetFromZero, -pushForce, dims3D);
                    applyForceOffset(room2.offsetFromZero, pushForce, dims3D);
                }
            }
        }
    }
}

// Helper

inline TileIndex getIndex2DAtPos(i32v2 pos, i32v2 dims, i32 floorIndex) {
    return (TileIndex)(floorIndex * dims.y * dims.x + pos.y * dims.x + pos.x);
}

inline TileIndex getIndex2DAtPos(i16v2 pos, i32v2 dims, i32 floorIndex) {
    return (TileIndex)(floorIndex * dims.y * dims.x + pos.y * dims.x + pos.x);
}

inline TileIndex getIndex2DAtPos(i32 x, i32 y, i32v2 dims, i32 floorIndex) {
    return (TileIndex)(floorIndex * dims.y * dims.x + y * dims.x + x);
}

inline i32v2 getPosAtIndex2D(TileIndex tileIndex, i32v2 dims) {
    i32 floorStride = dims.x * dims.y;
    return i32v2(tileIndex % dims.x, (tileIndex % floorStride) / dims.x);
}

inline f32 getPressureValue(const RoomNode& room) {
    return (f32)room.desiredSize / (f32)room.size;
}

constexpr ui8 ITER_STEP = 2;
constexpr ui8 MAX_WALL_LENGTH = 64;

const i32v2 WALL_EXPAND_OFFSETS[4] = {
    { 0, -1}, // SOUTH
    {-1,  0}, // WEST
    { 1,  0}, // EAST
    { 0,  1}  // NORTH
};

const i32v2 WALL_ITERATE_OFFSETS[4] = {
    { 1,  0}, // SOUTH
    { 0,  1}, // WEST
    { 0,  1}, // EAST
    { 1,  0}  // NORTH
};


void expandWall(Cartesian wallDir, BuildingBlueprintGenerationContext& context, RoomNode& room, VisualLog* visLog) {

    i32AABB2& roomAABB = room.aabb;
    i32v2 iterPos;
    i32 length;
    switch (wallDir) {
        case Cartesian::SOUTH:
            --roomAABB.pos.y;
            length = roomAABB.width;
            iterPos = roomAABB.pos;
            ++roomAABB.depth;
            break;
        case Cartesian::WEST:
            --roomAABB.pos.x;
            length = roomAABB.depth;
            iterPos = roomAABB.pos;
            ++roomAABB.width;
            break;
        case Cartesian::EAST:
            length = roomAABB.depth;
            iterPos = roomAABB.pos;
            iterPos.x += roomAABB.width;
            ++roomAABB.width;
            break;
        case Cartesian::NORTH:
            length = roomAABB.width;
            iterPos = roomAABB.pos;
            iterPos.y += roomAABB.depth;
            ++roomAABB.depth;
            break;
        default:
            assert(false);
            break;
    }

    // Set new metadata
    const i32v2& iterateOffset = WALL_ITERATE_OFFSETS[e_cast(wallDir)];
    // i32 tilesAdded = 0;
    for (int j = 0; j < length; ++j) {
        const i32 index = getIndex2DAtPos(iterPos, context.mTileSpatialGrid.getDims(), room.floorIndex);
        RoomNodeID ownerId = context.ownerArray[index];
        assert(ownerId == INVALID_ROOM_ID);
        //if (ownerId != INVALID_ROOM_ID) {
        //    RoomNode& ownerRoom = context.rooms[ownerId];
        //    // TODO: Can we optimize this so we don't run it every time?
        //}
        context.ownerArray[index] = room.id;
        context.tiles[index] = BlueprintTileType::FLOOR;

        if (visLog) {
            const color4& color = ROOM_COLORS[room.id % MAX_ROOM_COLORS];
            const f32v3 rootPos(iterPos.x, iterPos.y, room.floorIndex * context.mTileSpatialGrid.getFloorHeight());
            visLog->addWireQuad(rootPos + f32v3(0.1f, 0.1f, 0.0f), f32v2(0.8f), color);
        }
        // Step
        iterPos += iterateOffset;
    }
    room.size += length;
}

// Only fills gaps and will not overwrite any existing walls
void expandWallGapsOnly(Cartesian wallDir, BuildingBlueprintGenerationContext& context, RoomNode& room, VisualLog* visLog) {
    i32AABB2& aabb = room.aabb;
    i32v2 iterPos;
    i32 length;
    switch (wallDir) {
        case Cartesian::SOUTH:
            --aabb.pos.y;
            length = aabb.width;
            iterPos = aabb.pos;
            ++aabb.depth;
            break;
        case Cartesian::WEST:
            --aabb.pos.x;
            length = aabb.depth;
            iterPos = aabb.pos;
            ++aabb.width;
            break;
        case Cartesian::EAST:
            length = aabb.depth;
            iterPos = aabb.pos;
            iterPos.x += aabb.width;
            ++aabb.width;
            break;
        case Cartesian::NORTH:
            length = aabb.width;
            iterPos = aabb.pos;
            iterPos.y += aabb.depth;
            ++aabb.depth;
            break;
        default:
            assert(false);
            break;
    }

    // Set new metadata
    const i32v2& iterateOffset = WALL_ITERATE_OFFSETS[e_cast(wallDir)];
    i32 tilesAdded = 0;
    for (int j = 0; j < length; ++j) {
        // Only if we aren't an overwritten wall
        const i32 index = getIndex2DAtPos(iterPos, context.mTileSpatialGrid.getDims(), room.floorIndex);
        RoomNodeID ownerId = context.ownerArray[index];
        if (ownerId == INVALID_ROOM_ID) {
            ++tilesAdded;
            context.ownerArray[index] = room.id;
            context.tiles[index] = BlueprintTileType::FLOOR;
            // Visual log
            if (visLog) {
                visLog->addFilledQuad(f32v3(iterPos.x + 0.1f, iterPos.y + 0.1f, room.floorIndex * context.mTileSpatialGrid.getFloorHeight()), f32v2(0.8f), ROOM_COLORS[room.id % MAX_ROOM_COLORS]);
            }
        }
        // Step
        iterPos += iterateOffset;
    }

    // TODO: Only along actual expansion tiles
    // TODO: Decrement size when we push into another room
    room.size += tilesAdded;
}

bool expandRoomSquare(BuildingBlueprintGenerationContext& context, RoomNode& room, VisualLog* visLog) {

    
    bool didExpand = false;
    const i32v3& dims = context.mTileSpatialGrid.getDims();
    const f32 floorHeight = context.mTileSpatialGrid.getFloorHeight();
    i32AABB2& roomAABB = room.aabb;
    for (int i = 0; i < 4; ++i) {
        Cartesian wallDir = Cartesian(i);
        f32 currentPressure = getPressureValue(room);
        // Once we are at desired size, no need to expand
        if (currentPressure <= 1.0f) {
            return didExpand;
        }

        // Expand
        const WallInfo wallInfo = getWallInfoFromRoomAABB(wallDir, roomAABB);
        const i16v2& iterateOffset = WALL_ITERATE_OFFSETS[i];
        const int isY = (int)(wallDir == Cartesian::SOUTH || wallDir == Cartesian::NORTH);
        const i32v2 nextStart = i32v2(wallInfo.startPos) + WALL_EXPAND_OFFSETS[i];
        // Bounds check
        if (boundsCheckRoom(nextStart[isY], dims[isY])) {
            assert(wallInfo.length <= MAX_WALL_LENGTH);
            // We will only expand if we arent expanding into another room
            bool canExpand = true;
            i16v2 outerPos = nextStart;
            for (int j = 0; j < wallInfo.length; ++j) {
                const i32 index = getIndex2DAtPos(outerPos, dims, room.floorIndex);
                RoomNodeID ownerId = context.ownerArray[index];
                if (ownerId != INVALID_ROOM_ID) {
                    canExpand = false;
                    break;
                }
                // Step
                outerPos += iterateOffset;
            }
            // If we have room to expand, expand
            if (canExpand) {
                expandWall(wallDir, context, room, visLog);
                if (visLog) {
                    const f32v3 lineStart(wallInfo.startPos.x + 0.5f, wallInfo.startPos.y + 0.5f, room.floorIndex * floorHeight);
                    f32v3 lineEnd(wallInfo.startPos.x + 0.5f, wallInfo.startPos.y + 0.5f, room.floorIndex * floorHeight);
                    lineEnd.x += WALL_ITERATE_OFFSETS[i].x * wallInfo.length;
                    lineEnd.y += WALL_ITERATE_OFFSETS[i].y * wallInfo.length;
                    visLog->addLineBetweenPoints(lineStart, lineEnd, ROOM_COLORS[room.id % MAX_ROOM_COLORS]);
                }
                didExpand = true;
            }
        }
    }
   return didExpand;
}

bool expandRoomGaps(BuildingBlueprintGenerationContext& context, RoomNode& room, VisualLog* visLog) {

    bool didExpand = false;

    
    const i32v3& dims = context.mTileSpatialGrid.getDims();
    i32AABB2& roomAABB = room.aabb;
    for (int iter = 0; iter < ITER_STEP; ++iter) {
        i32 expandCount = 0;
        for (int i = 0; i < 4; ++i) {
            Cartesian wallDir = Cartesian(i);
            f32 currentPressure = getPressureValue(room);
            // Once we are at desired size, no need to expand
            if (currentPressure <= 1.0f) {
                return didExpand;
            }

            // Expand
            const WallInfo wallInfo = getWallInfoFromRoomAABB(wallDir, roomAABB);
            const i16v2& iterateOffset = WALL_ITERATE_OFFSETS[i];
            const int isY = (int)(wallDir == Cartesian::SOUTH || wallDir == Cartesian::NORTH);
            const i32v2 nextStart = i32v2(wallInfo.startPos) + WALL_EXPAND_OFFSETS[i];
            // Bounds check
            if (boundsCheckRoom(nextStart[isY], dims[isY])) {
                assert(wallInfo.length <= MAX_WALL_LENGTH);
                // We will only expand if we arent expanding into another room
                bool canExpand = false;
                i16v2 outerPos = nextStart;
                for (int j = 0; j < wallInfo.length; ++j) {
                    const i32 index = getIndex2DAtPos(outerPos, dims, room.floorIndex);
                    RoomNodeID ownerId = context.ownerArray[index];
                    if (ownerId == INVALID_ROOM_ID) {
                        // We need at least one gap to expand into
                        // TODO: Need to check for adjacent owned tile?
                        canExpand = true;
                        break;
                    }
                    // Step
                    outerPos += iterateOffset;
                }

                // If we have room to expand, expand
                if (canExpand) {
                    expandWallGapsOnly(wallDir, context, room, visLog);
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

void BuildingBlueprintGenerator::placeWalls(BuildingBlueprintGenerationContext& context, VisualLog* visLog) {
    if (visLog) visLog->nextStep("Place walls");
    
    const i32v3& dims = context.mTileSpatialGrid.getDims();
    const f32 floorHeight = context.mTileSpatialGrid.getFloorHeight();
    // First place main segments
    for (i32 z = 0; z < context.floorCount; ++z) {
        for (i32 y = 0; y < dims.y; ++y) {
            for (i32 x = 0; x < dims.x; ++x) {
                const i32 index = getIndex2DAtPos(x, y, dims, z);
                RoomNodeID roomId = context.ownerArray[index];
                if (roomId == INVALID_ROOM_ID) {
                    continue;
                }

                bool didSetWall = false;
                TileWall walls[4];
                // South
                if (y == 0 || context.ownerArray[getIndex2DAtPos(x, y - 1, dims, z)] != roomId) {
                    walls[e_cast(Cartesian::SOUTH)].wallID = context.tileIDs[e_cast(BlueprintTileType::WALL)];
                    if (visLog) {
                        visLog->addLineBetweenPoints(f32v3(x, y + 0.01f, z * floorHeight), f32v3(x + 1.0f, y + 0.01f, z * floorHeight), COLOR_WHITE);
                    }
                }
                // West
                if (x == 0 || context.ownerArray[getIndex2DAtPos(x - 1, y, dims, z)] != roomId) {
                    walls[e_cast(Cartesian::WEST)].wallID = context.tileIDs[e_cast(BlueprintTileType::WALL)];
                    if (visLog) {
                        visLog->addLineBetweenPoints(f32v3(x + 0.01f, y, z * floorHeight), f32v3(x + 0.01f, y + 1.0f, z * floorHeight), COLOR_WHITE);
                    }
                }
                // East
                if (x == dims.x - 1 || context.ownerArray[getIndex2DAtPos(x + 1, y, dims, z)] != roomId) {
                    walls[e_cast(Cartesian::EAST)].wallID = context.tileIDs[e_cast(BlueprintTileType::WALL)];
                    if (visLog) {
                        visLog->addLineBetweenPoints(f32v3(x + 1.0f - 0.01f, y, z * floorHeight), f32v3(x + 1.0f - 0.01f, y + 1.0f, z * floorHeight), COLOR_WHITE);
                    }
                }
                // North
                if (y == dims.y - 1 || context.ownerArray[getIndex2DAtPos(x, y + 1, dims, z)] != roomId) {
                    walls[e_cast(Cartesian::NORTH)].wallID = context.tileIDs[e_cast(BlueprintTileType::WALL)];
                    if (visLog) {
                        visLog->addLineBetweenPoints(f32v3(x, y + 1.0f - 0.01f, z * floorHeight), f32v3(x + 1.0f, y + 1.0f - 0.01f, z * floorHeight), COLOR_WHITE);
                    }
                }
                context.walls.setWallsAtTile(index, walls);
            }
        }
    }
}


void BuildingBlueprintGenerator::allocateTileData(BuildingBlueprintGenerationContext& context) {
    
    const i32v2 dims2D = context.mTileSpatialGrid.getDims2D();
    ui32 numTiles = dims2D.x * dims2D.y * context.floorCount;
    // Set proper floor count into the AABB
    context.mTileSpatialGrid.init(context.mTileSpatialGrid.getWorldPos3D(), i32v3(dims2D.x, dims2D.y, context.floorCount), context.mTileSpatialGrid.getFloorHeight());
    context.tiles.resize((size_t)context.floorCount * dims2D.x * dims2D.y, BlueprintTileType::NONE);
    context.walls.init(&context.mTileSpatialGrid);
    context.ownerArray.resize(context.tiles.size(), INVALID_ROOM_ID);
}

void BuildingBlueprintGenerator::expandRooms(BuildingBlueprintGenerationContext& context, VisualLog* visLog) {
    if (visLog) visLog->nextStep("Expand rooms");
    
    const i32v3& dims = context.mTileSpatialGrid.getDims();

    // Init rooms
    for (size_t i = 0; i < context.rooms.size(); ++i) {
        if (visLog) {
            f32v3 pos(context.rooms[i].offsetFromZero.x, context.rooms[i].offsetFromZero.y, context.rooms[i].floorIndex * context.mTileSpatialGrid.getFloorHeight());
            visLog->addWireQuad(pos, f32v2(1.0f), ROOM_COLORS[context.rooms[i].id % MAX_ROOM_COLORS]);
        }
        initRoomWalls(context, context.rooms[i]);
    }
    // Expand one floor at a time so that second story rooms can copy their
    // lower parent layout for easier stair placement
    boost::container::static_vector<RoomNode*, 256> roomsOnThisFloorToExpand;

    for (i32 z = 0; z < context.floorCount; ++z) {
        // Collect rooms, direct copy any rooms that are connected to parent via stairs
        roomsOnThisFloorToExpand.clear();
        for (auto&& room : context.rooms) {
            if (room.floorIndex == z) {
                if (room.connectedToParentWithStairs) {
                    assert(z != 0);
                    // Direct copy!
                    RoomNode& parent = context.rooms[room.parentRoom];
                    room.aabb = parent.aabb;
                    // Iterate over every tile on the floor to copy
                    // TODO: AABB iterate instead
                    for (i32 y = 0; y < dims.y; ++y) {
                        for (i32 x = 0; x < dims.x; ++x) {
                            TileIndex myIndex = getIndex2DAtPos(x, y, dims, z);
                            TileIndex parentIndex = myIndex - dims.x * dims.y;
                            if (context.ownerArray[parentIndex] == parent.id) {
                                context.ownerArray[myIndex] = room.id;
                                context.tiles[myIndex] = context.tiles[parentIndex];
                                room.size = parent.size;
                            }
                        }
                    }

                }
                else {
                    roomsOnThisFloorToExpand.push_back(&room);
                }
            }
        }

        // Expand walls in square shape, no overwrite
        for (int iters = 0; iters < MAX_WALL_LENGTH; ++iters) {
            int failCount = 0;
            for (auto&& room : roomsOnThisFloorToExpand) {
                failCount += expandRoomSquare(context, *room, visLog) ? 0 : 1;
            }
            if (failCount == roomsOnThisFloorToExpand.size()) {
                break;
            }
        }
        // Fill in gaps, no overwrite
        for (int iters = 0; iters < MAX_WALL_LENGTH / ITER_STEP; ++iters) {
            int failCount = 0;
            for (auto&& room : roomsOnThisFloorToExpand) {
                failCount += expandRoomGaps(context, *room, visLog) ? 0 : 1;
            }
            if (failCount == roomsOnThisFloorToExpand.size()) {
                break;
            }
        }
    }

    // Fixup room offsets
}

// Cellular Automata Sub-steps

ui8 ROOM_NODE_COUNT_CACHE[0xff + 0x1] = {};

// NOTE: Iteration order is important! We iterate to the right and then upwards
void FixupSingleRoomPieces(BuildingBlueprintGenerationContext& context, i32 x, i32 y, i32 index, VisualLog* visLog) {

    bool iterateAgain;
    
    const f32 floorHeight = context.mTileSpatialGrid.getFloorHeight();
    const i32v3& dims = context.mTileSpatialGrid.getDims();

    do {
        Cartesian bestDir;
        iterateAgain = false;
        // Order doesnt matter here so go in cache-order
        const RoomNodeID myID = context.ownerArray[index];
        if (myID == INVALID_ROOM_ID) {
            return;
        }

        ++ROOM_NODE_COUNT_CACHE[myID];
        // Down
        const RoomNodeID downID = context.ownerArray[(i32)(index - dims.x)];
        ++ROOM_NODE_COUNT_CACHE[downID];
        // Left
        const RoomNodeID leftID = context.ownerArray[(i32)(index - 1)];
        ++ROOM_NODE_COUNT_CACHE[leftID];
        // Right
        const RoomNodeID rightID = context.ownerArray[(i32)(index + 1)];
        ++ROOM_NODE_COUNT_CACHE[rightID];
        // Top
        const RoomNodeID topID = context.ownerArray[(i32)(index + dims.x)];
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
                bestDir = Cartesian::SOUTH;
            }
            // Left
            const ui8 leftCount = ROOM_NODE_COUNT_CACHE[leftID];
            if (leftCount > bestCount) {
                bestId = leftID;
                bestCount = leftCount;
                iterateAgain = true;
                bestDir = Cartesian::WEST;
            }

            // Right
            const ui8 rightCount = ROOM_NODE_COUNT_CACHE[rightID];
            if (rightCount > bestCount) {
                bestId = rightID;
                bestCount = rightCount;
                iterateAgain = true;
                bestDir = Cartesian::EAST;
            }

            // Top
            const ui8 topCount = ROOM_NODE_COUNT_CACHE[topID];
            if (topCount > bestCount) {
                bestId = topID;
                bestCount = topCount;
                iterateAgain = true;
                bestDir = Cartesian::NORTH;
            }

            // Replace!
            if (bestId != myID) {
                // Visual log
                if (bestId != INVALID_ROOM_ID) {
                    ++context.rooms[bestId].size;
                    if (visLog) {
                        visLog->addFilledQuad(f32v3(x, y, context.rooms[bestId].floorIndex * floorHeight), f32v2(1.0f), ROOM_COLORS[bestId % MAX_ROOM_COLORS]);
                    }
                }
                else {
                    // Clear any tile data here
                    if (visLog) {
                        visLog->addFilledQuad(f32v3(x, y, context.rooms[myID].floorIndex * floorHeight), f32v2(1.0f), COLOR_GRAY);
                    }
                    context.tiles[index] = BlueprintTileType::NONE;
                }

                context.ownerArray[index] = bestId;
                --context.rooms[myID].size;
            }
            else {
                assert(myID != INVALID_ROOM_ID);
                // Visual log
                if (visLog) {
                    visLog->addFilledQuad(f32v3(x, y, context.rooms[myID].floorIndex * floorHeight), f32v2(1.0f), COLOR_RED);
                }
                context.tiles[index] = BlueprintTileType::NONE;
                context.ownerArray[index] = INVALID_ROOM_ID;
                --context.rooms[myID].size;
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
                case Cartesian::SOUTH: // down
                    if (y == 1) {
                        return;
                    }
                    --y;
                    index -= dims.x;
                    break;
                case Cartesian::WEST: // left
                    if (x == 1) {
                        return;
                    }
                    --x;
                    --index;
                    break;
                case Cartesian::EAST: // right
                    if (x == dims.x - 2) {
                        return;
                    }
                    ++x;
                    ++index;
                    break;
                case Cartesian::NORTH: // up
                    if (y == dims.y - 2) {
                        return;
                    }
                    ++y;
                    index += dims.x;
                    break;
            }
        }
        else {
            return;
        }
    } while (true);
}

// Sweeping Cellular Automata
void CellularAutomataSubStepThickenPassages(BuildingBlueprintGenerationContext& context, i32 x, i32 y, i32 index, RoomNode& room) {

    // Order matters here, we want to thicken ahead of us and create a wave.
    // With this order, we are allowed to modify Right, up, and ourselves, but not down or left.

    // When expanding, use a pressure function.
    const f32 pressure = getPressureValue(room); // Outward - If > 1, then we are trying to grow. If < 1, then we are trying to shrink
   

    // CAN ONLY MODIFY OURSELVES
    // Right
    //if (x < context.dims.x) {
    //    RoomNodeID& rightTile = context.ownerArray[index + 1];
    //    f32 currentPressure;
    //    // Always expand into invalid
    //    if (rightTile != INVALID_ROOM_ID) {
    //        RoomNode& rightRoom = context.nodes[rightTile];
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

void BuildingBlueprintGenerator::roomCleanup(BuildingBlueprintGenerationContext& context, VisualLog* visLog)
{
    constexpr int CELLULAR_AUTOMATA_ITERATIONS = 1;

    if (visLog) visLog->nextStep("Room cleanup");
    
    const i32v3& dims = context.mTileSpatialGrid.getDims();

    for (int step = 0; step < CELLULAR_AUTOMATA_ITERATIONS; ++step) {
        for (i32 z = 0; z < context.floorCount; ++z) {
            for (i32 y = 1; y < dims.y - 1; ++y) {
                for (i32 x = 1; x < dims.x - 1; ++x) {
                    const i32 index = getIndex2DAtPos(x, y, dims, z);
                    FixupSingleRoomPieces(context, x, y, index, visLog);
                }
            }
        }
    }
}

void BuildingBlueprintGenerator::computeRoomAABBs(BuildingBlueprintGenerationContext& context, VisualLog* visLog) {
    if (visLog) visLog->nextStep("Compute AABBs");

    std::unordered_map<RoomNodeID, i32v4 /* xspan, yspan */ > roomBoundsLookup;
    roomBoundsLookup.reserve(20);

    // Reserve
    
    for (size_t i = 0; i < context.rooms.size(); ++i) {
        RoomNode& room = context.rooms[i];
        room.tilePositions.reserve(room.size);
    }

    const i32v3& rootPos = context.mTileSpatialGrid.getWorldPos3D();
    const i32v3& dims = context.mTileSpatialGrid.getDims();
    const f32 floorHeight = context.mTileSpatialGrid.getFloorHeight();
    for (TileIndex tileIndex = 0; tileIndex < (TileIndex)context.tiles.size(); ++tileIndex) {
        // Compute bounds
        const RoomNodeID id = context.ownerArray[tileIndex];
        if (id != INVALID_ROOM_ID) {

            const i32v2 pos = getPosAtIndex2D(tileIndex, dims);

            if (visLog) {
                const color4& color = ROOM_COLORS[id % MAX_ROOM_COLORS];
                const f32v3 rootPos(pos.x, pos.y, context.rooms[id].floorIndex * floorHeight);
                visLog->addWireQuad(rootPos + f32v3(0.1f, 0.1f, 0.0f), f32v2(0.8f), color);
            }

            context.rooms[id].tilePositions.emplace_back(tileIndex);
            auto&& it = roomBoundsLookup.find(id);
            if (it == roomBoundsLookup.end()) {
                roomBoundsLookup[id] = i32v4(pos.x, pos.x, pos.y, pos.y);
            }
            else {
                // AABB bounds
                if (pos.x < it->second.x) {
                    it->second.x = pos.x;
                }
                else if (pos.x > it->second.y) {
                    it->second.y = pos.x;
                }
                if (pos.y < it->second.z) {
                    it->second.z = pos.y;
                }
                else if (pos.y > it->second.w) {
                    it->second.w = pos.y;
                }
            }
        }
    }

    // Set up true AABBs and update offsetFromZero
    for (auto&& it : roomBoundsLookup) {
        RoomNode& room = context.rooms[it.first];
        // i32AABB2 oldAABB = room.aabb;
        room.aabb.pos.x = it.second.x + rootPos.x;
        room.aabb.dims.x = it.second.y - it.second.x + 1;
        room.aabb.pos.y = it.second.z + rootPos.y;
        room.aabb.dims.y = it.second.w - it.second.z + 1;
        room.offsetFromZero.x = it.second.x;
        room.offsetFromZero.y = it.second.z;
        if (visLog) {
            const color4& color = ROOM_COLORS[room.id % MAX_ROOM_COLORS];
            const f32v3 rootPos(room.offsetFromZero.x, room.offsetFromZero.y, room.floorIndex * floorHeight);
            visLog->addWireQuad(rootPos, f32v2(1.0f), color);
            const f32v3 origin(it.second.x, it.second.z, room.floorIndex * floorHeight);
            visLog->addWireQuad(origin, f32v2(room.aabb.dims), ROOM_COLORS[room.id % MAX_ROOM_COLORS]);
        }
        //assert(oldAABB == room.aabb); true AABB whereas oldAABB is relative AABB
    }

    // Remove extra memory
    for (RoomNode& room : context.rooms) {
        room.tilePositions.shrink_to_fit();
    }

}

bool BuildingBlueprintGenerator::validateRoomsArentEmpty(BuildingBlueprintGenerationContext& context, VisualLog* visLog) {
    for (const RoomNode& room : context.rooms) {
        if (room.tilePositions.empty()) {
            if (visLog) {
                char buf[64];
                room.roomDef->getName().toString(buf, nullptr);
                visLog->addText("INVALID ROOM: " + std::string(buf), f32v3(0.0f), 1.0f, f32v2(0.0f, 1.0f), COLOR_RED);
            }
            return false;
        }
    }
    return true;
}

void BuildingBlueprintGenerator::initRoomWalls(BuildingBlueprintGenerationContext& context, RoomNode& room)
{
    const i32 index = getIndex2DAtPos(i32v2(room.offsetFromZero), context.mTileSpatialGrid.getDims2D(), room.floorIndex);
    // Init root node
    room.size = 1;
    context.tiles[index] = BlueprintTileType::FLOOR;
    context.ownerArray[index] = room.id;
    assert(room.id != INVALID_ROOM_ID);

    // Init aabb
    i32AABB2& aabb = room.aabb;
    aabb.pos = room.offsetFromZero;
    aabb.width = 1;
    aabb.depth = 1;
}

struct DoorBFSNode {
    i32 index;
};

void doorBfs(std::vector<DoorBFSNode>& bfs, size_t& bfsBackIndex, BuildingBlueprintGenerationContext& context, Cartesian dir, TileIndex tileIndex, RoomNode& room, const i32v2& currentPos, BitArray& visited, BitArray& isConnected, bool& canConnectToOutside, VisualLog* visLog) {
    const i32v2& directionOffset = WALL_EXPAND_OFFSETS[e_cast(dir)];
    const i32v2 nextPos = currentPos + directionOffset;
    const TileIndex nextTileIndex = getIndex2DAtPos(i32v2(nextPos), context.mTileSpatialGrid.getDims2D(), room.floorIndex);
    const f32 floorHeight = context.mTileSpatialGrid.getFloorHeight();
    const i32v3& dims = context.mTileSpatialGrid.getDims();
    if (!visited.getBit(nextTileIndex)) {
        RoomNodeID owner = context.ownerArray[tileIndex];
        RoomNodeID nextOwner = context.ownerArray[nextTileIndex];
        visited.setBit(nextTileIndex);
        if (nextOwner == owner) {
            bfs[bfsBackIndex++].index = nextTileIndex;
            if (bfsBackIndex >= bfs.size()) bfsBackIndex = 0;
        }
        else if (context.tiles[tileIndex] == BlueprintTileType::FLOOR) {

            if (nextOwner == INVALID_ROOM_ID) {
                // Exterior doors
                if (canConnectToOutside) {
                    canConnectToOutside = false;
                    TileWall wall;
                    wall.wallID = context.tileIDs[e_cast(BlueprintTileType::DOOR)];
                    wall.isDoor = true;
                    context.walls.setWallAtTile(tileIndex, wall, dir);
                    context.exteriorDoors[tileIndex] = context.ownerArray[tileIndex];
                    // Visual log
                    if (visLog) {
                        const f32v3 pos(tileIndex % dims.x, tileIndex % (dims.x * dims.y) / dims.x, (tileIndex / (dims.x * dims.y)) * floorHeight);
                        visLog->addFilledQuad(pos, f32v2(1.0f), color4(1.0f, 0.0f, 0.0f, 0.8f));
                    }
                }
            }
            else if (!isConnected.getBit(nextOwner) && room.numAdjacentRooms < MAX_ADJACENT_ROOMS) {
                // Interior doors
                RoomNode& adjacent = context.rooms[nextOwner];
                if (adjacent.numAdjacentRooms < MAX_ADJACENT_ROOMS) {

                    if (context.tiles[nextTileIndex] == BlueprintTileType::FLOOR) {

                        TileWall wall;
                        wall.wallID = context.tileIDs[e_cast(BlueprintTileType::DOOR)];
                        wall.isDoor = true;
                        context.walls.setWallAtTile(tileIndex, wall, dir);
                      
                        // Clear out any wall on opposite side
                        isConnected.setBit(adjacent.id);
                        room.adjacentRooms[room.numAdjacentRooms++] = RoomGateInfo{ adjacent.id, nextTileIndex };
                        adjacent.adjacentRooms[adjacent.numAdjacentRooms++] = RoomGateInfo{ context.ownerArray[tileIndex], tileIndex };
                        // Visual log
                        if (visLog) {
                            const f32v3 pos(tileIndex % dims.x, tileIndex % (dims.x * dims.y) / dims.x, (tileIndex / (dims.x * dims.y)) * floorHeight);
                            visLog->addFilledQuad(pos, f32v2(1.0f), color4(0.0f, 1.0f, 1.0f, 0.8f));
                        }
                    }
                }
            }
        }
    }
}

void BuildingBlueprintGenerator::placeDoors(BuildingBlueprintGenerationContext& context, VisualLog* visLog) {
    if (visLog) visLog->nextStep("Place doors");
    // TODO: Re-use memory
    
    BitArray visited(context.tiles.size());
    BitArray isConnected(context.rooms.size());

    const i32v3& dims = context.mTileSpatialGrid.getDims();
    const f32 floorHeight = context.mTileSpatialGrid.getFloorHeight();

    // Ringbuffer
    // TODO: Re-use memory
    std::vector<DoorBFSNode> bfs(context.tiles.size());
    size_t bfsFrontIndex;
    size_t bfsBackIndex;
    bool canConnectToOutside = true;
    for (auto&& room : context.rooms) {
        assert(room.size);
        // Clear visited list
        visited.zeroAllBits();
        isConnected.zeroAllBits();

        // Mark neighbor rooms as connected
        for (int i = 0; i < room.numAdjacentRooms; ++i) {
            isConnected.setBit(room.adjacentRooms[i].adjacentRoom);
        }

        bfsFrontIndex = 0;
        bfsBackIndex = 1;

        // Random tile to start
        const i32 startIndex = (i32)room.tilePositions[context.randomGen->getRandomUint() % room.tilePositions.size()];
        assert(context.ownerArray[startIndex] == room.id);
        // Visual log
        if (visLog) {
            const i32 floorStride = dims.x * dims.y;
            const f32v3 offset(startIndex % dims.x, (startIndex % floorStride) / dims.x, startIndex / floorStride);
            visLog->addFilledQuad(offset, f32v2(1.0f), ROOM_COLORS[room.id % MAX_ROOM_COLORS]);
        }
        visited.setBit(startIndex);
        bfs[bfsFrontIndex].index = startIndex;
        const i32 floorSize = dims.x * dims.y;
        // Do the bfs
        while (bfsFrontIndex != bfsBackIndex) {
            const DoorBFSNode& node = bfs[bfsFrontIndex];
            i32v2 pos(node.index % dims.x, (node.index % floorSize) / dims.x);
            RoomNodeID roomId = context.ownerArray[node.index];
            assert(roomId == room.id);
            if (room.numAdjacentRooms == MAX_ADJACENT_ROOMS) {
                break;
            }

            if (visLog) {
                visLog->addWireQuad(f32v3(pos.x, pos.y, room.floorIndex * floorHeight), f32v2(1.0f), color4(0.8f, 0.8f, 0.8f, 0.5f));
            }

            // Bottom
            if (pos.y > 0) {
                doorBfs(bfs, bfsBackIndex, context, Cartesian::SOUTH, node.index, room, pos, visited, isConnected, canConnectToOutside, visLog);
            }

            // Left
            if (pos.x > 0) {
                doorBfs(bfs, bfsBackIndex, context, Cartesian::WEST, node.index, room, pos, visited, isConnected, canConnectToOutside, visLog);
            }

            // Right
            if (pos.x < dims.x - 1) {
                doorBfs(bfs, bfsBackIndex, context, Cartesian::EAST, node.index, room, pos, visited, isConnected, canConnectToOutside, visLog);
            }

            // Up
            if (pos.y < dims.y - 1) {
                doorBfs(bfs, bfsBackIndex, context, Cartesian::NORTH, node.index, room, pos, visited, isConnected, canConnectToOutside, visLog);
            }
            ++bfsFrontIndex;
        }
    }
}

void BuildingBlueprintGenerator::buildRoomInteriorEdges(BuildingBlueprintGenerationContext& context, VisualLog* visLog) {
    PreciseTimer timer;
    if (visLog) visLog->nextStep("Build interior edges");
    // TODO: Room minimum AABB?
    BitArray bits;
    
    const f32 floorHeight = context.mTileSpatialGrid.getFloorHeight();
    const i32v2& floorDims = context.mTileSpatialGrid.getDims2D();
    bits.resize(floorDims.x * floorDims.y);
    for (auto&& room : context.rooms) {
   
        TileIndex tileIndex = room.floorIndex * floorDims.x * floorDims.y;
        for (i32 y = 0; y < floorDims.y; ++y) {
            for (i32 x = 0; x < floorDims.x; ++x) {
                bits.setBitTo(y * floorDims.x + x, context.ownerArray[tileIndex] == room.id && context.tiles[tileIndex] == BlueprintTileType::FLOOR);
                ++tileIndex;
            }
        }
        room.interiorEdges = GridEdgeFinder::getInteriorEdgesFromOwnershipArray(bits, floorDims, visLog, room.floorIndex * floorHeight);
        room.edgeWalk = GridEdgeFinder::getInteriorCounterClockwiseWalkFromGridEdges(room.interiorEdges, floorDims);
    }

    if (visLog) {
        visLog->nextStep("Debug interior edges");
        for (auto&& room : context.rooms) {
            for (auto&& edge : room.interiorEdges) {
                if (edge.edgeDir == Cartesian::SOUTH) {
                    i32v2 pos = getPosAtIndex2D(edge.start, floorDims);
                    visLog->addWireQuad(f32v3(pos.x, pos.y, room.floorIndex * floorHeight), f32v2(edge.length, 1.0f), color4(0.0f, 1.0f, 1.0f, 0.9f));
                }
                else if (edge.edgeDir == Cartesian::NORTH) {
                    i32v2 pos = getPosAtIndex2D(edge.end, floorDims);
                    visLog->addWireQuad(f32v3(pos.x, pos.y, room.floorIndex * floorHeight), f32v2(edge.length, 1.0f), color4(0.0f, 1.0f, 0.0f, 0.9f));
                }
                else if (edge.edgeDir == Cartesian::WEST) {
                    i32v2 pos = getPosAtIndex2D(edge.end, floorDims);
                    visLog->addWireQuad(f32v3(pos.x, pos.y, room.floorIndex * floorHeight), f32v2(1.0f, edge.length), color4(1.0f, 0.0f, 1.0f, 0.9f));
                }
                else if (edge.edgeDir == Cartesian::EAST) {
                    i32v2 pos = getPosAtIndex2D(edge.start, floorDims);
                    visLog->addWireQuad(f32v3(pos.x, pos.y, room.floorIndex * floorHeight), f32v2(1.0f, edge.length), color4(1.0f, 0.0f, 0.0f, 0.9f));
                }
            }
        }
    }

    if (visLog) {
        visLog->nextStep("Debug edge walk");
        for (auto&& room : context.rooms) {
            for (auto&& index : room.edgeWalk) {
                i32v2 pos = getPosAtIndex2D(index, floorDims);
                visLog->addWireQuad(f32v3(pos.x, pos.y, room.floorIndex * floorHeight), f32v2(1.0f), color4(1.0f, 1.0f, 1.0f, 0.9f));
            }
        }
    }

}

inline bool isDoor(TileID tileId) {
    return tileId != TILE_ID_NONE && TileRepository::get().getLoadedOrUnloadedAsset(tileId).shape == TileShape::DOOR;
}

bool tileBlocksDoor(TileIndex index, BuildingBlueprintGenerationContext& context) {
    const i32v2& floorDims = context.mTileSpatialGrid.getDims2D();
    const i32v2 pos = getPosAtIndex2D(index, floorDims);
    assert(pos.x > 0 && pos.x < floorDims.x - 1 && pos.y > 0 && pos.y < floorDims.y - 1); // We should have a wall buffer guarenteed
    TileWalls walls;
    context.walls.getWallsAtTile(walls, index);
    for (int i = 0; i < 4; ++i) {
        if (isDoor(walls.walls[i].wallID)) {
            return true;
        }
    }
    
    return false;
}

bool canPlaceStairsHere(TileIndex index, BuildingBlueprintGenerationContext& context, RoomNode& child) {
    const i32v2& floorDims = context.mTileSpatialGrid.getDims2D();
    const TileIndex aboveIndex = index + floorDims.x * floorDims.y;
    if (context.tiles[index] == BlueprintTileType::FLOOR &&
        context.ownerArray[aboveIndex] == child.id &&
        context.tiles[aboveIndex] == BlueprintTileType::FLOOR &&
        !tileBlocksDoor(index, context) && !tileBlocksDoor(aboveIndex, context)) {
        return true;
    }
    return false;
}

bool isAtWallCorner(TileIndex index, BuildingBlueprintGenerationContext& context) {
    
    const i32v2& floorDims = context.mTileSpatialGrid.getDims2D();
    const i32v2 pos = getPosAtIndex2D(index, floorDims);
    assert(pos.x > 0 && pos.x < floorDims.x - 1 && pos.y > 0 && pos.y < floorDims.y - 1); // We should have a wall buffer guarenteed
    i32 adjacentWallCount = 0;
    TileWalls walls;
    context.walls.getWallsAtTile(walls, index);
    for (int i = 0; i < 4; ++i) {
        if (walls.walls[i].isValid()) {
            ++adjacentWallCount;
        }
    }
    // TODO: This disallows single block hallways
    return adjacentWallCount >= 2;
}

bool isRunningIntoWallAtEnd(TileIndex index, BuildingBlueprintGenerationContext& context, Cartesian dir) {
    
    const i32v2& floorDims = context.mTileSpatialGrid.getDims2D();
    i16v2 pos = getPosAtIndex2D(index, floorDims);
    const i32 floorIndex = index / (floorDims.x * floorDims.y);
    assert(pos.x > 0 && pos.x < floorDims.x - 1 && pos.y > 0 && pos.y < floorDims.y - 1); // We should have a wall buffer guarenteed
    pos += CARTESIAN_NORMALS_2D[e_cast(dir)];
    return context.tiles[getIndex2DAtPos(pos, floorDims, floorIndex + 1)] != BlueprintTileType::FLOOR;
}

void BuildingBlueprintGenerator::placeStairs(BuildingBlueprintGenerationContext& context, VisualLog* visLog) {
    if (visLog) visLog->nextStep("Place stairs");

    boost::container::static_vector<TileIndex, 256> runs;
    boost::container::static_vector<ui8, 256> runStarts;
    boost::container::static_vector<ui8, 256> runLengths;
    boost::container::static_vector<Cartesian, 256> dirs;
    
    const i32v2& floorDims = context.mTileSpatialGrid.getDims2D();
    const i32 floorSize = floorDims.x * floorDims.y;
    const f32 floorHeight = context.mTileSpatialGrid.getFloorHeight();
    BitArray usedTiles;
    usedTiles.resize(floorSize);
    for (auto&& room : context.rooms) {
        runs.clear();
        runStarts.clear();
        runLengths.clear();
        dirs.clear();
        usedTiles.zeroAllBits();
        if (!room.hasStairs) {
            continue;
        }

        RoomNode* child = nullptr;
        for (auto&& id : room.childRooms) {
            if (context.rooms[id].floorIndex == room.floorIndex + 1) {
                child = &context.rooms[id];
                break;
            }
        }
        assert(child != nullptr);

        // Find stairs placement runs
        i32 currentRunLength = 0;
        i32 i = 0;
        while (true) {
            const i32 bitIndex = room.edgeWalk[i];
            const TileIndex index = bitIndex + room.floorIndex * floorSize;
            // Valid stair placement?
            if (currentRunLength < room.edgeWalk.size() && canPlaceStairsHere(index, context, *child) &&
                (currentRunLength == 0 || index != runs.back() /*make sure we dont double back*/) &&
                (currentRunLength != 0 || !isAtWallCorner(index, context))) {
                // New run?
                if (currentRunLength == 0) {
                    if (usedTiles.getBit(bitIndex)) {
                        // If this is a brand new run on an already used tile, were done
                        assert(runStarts.size() == runLengths.size());
                        break; // <== LOOP EXIT
                    }
                    else {
                        // Begin new run
                        runStarts.push_back(runs.size());
                    }
                }
                runs.push_back(index);
                ++currentRunLength;
                if (visLog) {
                    const i32v2 pos = getPosAtIndex2D(index, floorDims);
                    visLog->addFilledQuad(f32v3(pos.x, pos.y, room.floorIndex * floorHeight), f32v2(1.0f), color4(1.0f, 1.0f, 1.0f, 0.25f));
                }
            }
            else {
                if (currentRunLength) {
                    runLengths.push_back(currentRunLength);
                } else if (usedTiles.getBit(bitIndex)) {
                    assert(runStarts.size() == runLengths.size());
                    break; // <== LOOP EXIT
                }
                currentRunLength = 0;
                if (visLog) {
                    const i32v2 pos = getPosAtIndex2D(index, floorDims);
                    visLog->addWireQuad(f32v3(pos.x, pos.y, room.floorIndex * floorHeight), f32v2(1.0f), color4(0.7f, 0.0f, 0.0f, 0.9f));
                }
            }
            usedTiles.setBit(bitIndex);
            ++i;
            // Looparound
            if (i == room.edgeWalk.size()) {
                i = 0;
            }
        }
        assert(currentRunLength == 0);
        // Enumerate runs and find the best/most valid one for stairs
        const i32 STAIRS_UP_COUNT = context.mTileSpatialGrid.getFloorHeight() + 1;
        i32 bestRunStart = INT32_MAX;
        i32 bestRunLength = INT32_MAX;
        if (runs.size()) {
            dirs.resize(runs.size());
            for (size_t i = 0; i < runStarts.size(); ++i) {
                const i32 runStart = runStarts[i];
                const i32 runLength = runLengths[i];
                if (runLength < STAIRS_UP_COUNT) {
                    continue;
                }

                Cartesian prevDir = Cartesian::NONE;
                Cartesian dir = Cartesian::NONE;
                
                i32 upCount = 0;
                bool wasFlat = false;
                i32 j;
                for (j = 0; j < runLength - 1; ++j) {
                    // Look ahead for direction
                    const TileIndex index = runs[(i32)(runStart + j)];
                    const i32v2 pos = getPosAtIndex2D(index, floorDims);
                    const i32v2 nextPos = getPosAtIndex2D(runs[(i32)(runStart + j + 1)], floorDims);
                    if (nextPos.x > pos.x) {
                        dir = Cartesian::EAST;
                        assert(prevDir != Cartesian::WEST);
                    }
                    else if (nextPos.y > pos.y) {
                        dir = Cartesian::NORTH;
                        assert(prevDir != Cartesian::SOUTH);
                    }
                    else if (nextPos.x < pos.x) {
                        dir = Cartesian::WEST;
                        assert(prevDir != Cartesian::EAST);
                    }
                    else {
                        dir = Cartesian::SOUTH;
                        assert(prevDir != Cartesian::NORTH);
                    }
                    if (visLog) {
                        visLog->addFilledQuad(f32v3(pos.x, pos.y, room.floorIndex * floorHeight), f32v2(1.0f), CARTESIAN_COLORS[e_cast(dir)]);
                    }
                    // Store dir
                    dirs[(i32)(runStart + j)] = dir;
                    if (prevDir == dir || prevDir == Cartesian::NONE) {
                        ++upCount;
                        // Valid!
                        if (upCount == STAIRS_UP_COUNT) {
                            // If we are running into a wall on the next floor, we arent done
                            if (isRunningIntoWallAtEnd(index, context, dir)) {
                                --upCount;
                            }
                            else {
                                // Check if this is the best
                                if (j < bestRunLength) {
                                    bestRunStart = runStart;
                                    bestRunLength = j + 1;
                                    if (visLog) {
                                        visLog->addWireQuad(f32v3(pos.x, pos.y, room.floorIndex * floorHeight), f32v2(1.0f), color4(1.0f, 0.0f, 0.0f, 1.0f));
                                    }
                                }

                                break;
                            }
                        }
                        if (visLog) {
                            visLog->addWireQuad(f32v3(pos.x, pos.y, room.floorIndex * floorHeight), f32v2(1.0f), color4(1.0f, 1.0f, 1.0f, 1.0f));
                        }
                    }
                    else {
                        if (visLog) {
                            visLog->addWireQuad(f32v3(pos.x, pos.y, room.floorIndex * floorHeight), f32v2(1.0f), color4(0.0f, 0.0f, 1.0f, 1.0f));
                        }
                    }

                    prevDir = dir;
                }
                // Last one is up always (if not into a wall)
                if (upCount == STAIRS_UP_COUNT - 1) {
                    dirs[(i32)(runStart + j)] = dir;
                    // If running into a wall, not done
                    if (!isRunningIntoWallAtEnd(runs[(i32)(runStart + j)], context, dir)) {
                        // Check if this is the best
                        if (j < bestRunLength) {
                            bestRunStart = runStart;
                            bestRunLength = j + 1;
                        }
                        break;
                    }
                }

            }
        }
        else {
            continue; // Invalid! TODO: Ladder?
        }
        if (bestRunStart == INT32_MAX) {
            continue; // No run found! TODO: Ladder?
        }
        // Add stair pieces
        auto& stairs = context.stairs.emplace_back();
        stairs.reserve(stairs.size() + bestRunLength);
        i32 height = 0;
        Cartesian prevDir = Cartesian::NONE;
        for (i32 j = 0; j < bestRunLength; ++j) {
            const TileIndex tileIndex = runs[bestRunStart + j];
            context.tiles[tileIndex] = BlueprintTileType::STAIRS;
            if (j > 0) {
                context.tiles[tileIndex + floorSize] = BlueprintTileType::AIR;
            }
            StairPiece& stairPiece = stairs.emplace_back(StairPiece{});
            stairPiece.dir = dirs[bestRunStart + j];
            stairPiece.pos = tileIndex;
            stairPiece.isLastPiece = (j == bestRunLength - 1);
            if (prevDir != Cartesian::NONE && stairPiece.dir != prevDir) {
                stairPiece.height = height;
                stairPiece.isFlatPart = true;
            }
            else {
                stairPiece.height = height++;
            }
            prevDir = stairPiece.dir;
            if (visLog) {
                const i32v2 pos = getPosAtIndex2D(runs[bestRunStart + j], floorDims);
                const f32v3 visPos = f32v3(pos.x, pos.y, room.floorIndex * floorHeight);
                visLog->addFilledQuad(visPos, f32v2(1.0f), color4(0.0f, 1.0f, 0.0f, 0.9f));
                visLog->addCartesianArrow(visPos + f32v3(0.5f, 0.5f, 0.0f), 0.75f, CARTESIAN_COLORS[e_cast(stairPiece.dir)], stairPiece.dir);
            }
        }
    }
}

void BuildingBlueprintGenerator::buildExteriorWallRuns(BuildingBlueprintGenerationContext& context, VisualLog* visLog) {

    if (visLog) visLog->nextStep("Exterior Wall Runs");

    
    std::vector<ExteriorWallRun>& exteriorWallRuns = context.exteriorWallRuns;
    exteriorWallRuns.reserve(32);

    const i32v2& dims = context.mTileSpatialGrid.getDims2D();
    const i32 floorSize = dims.x * dims.y;
    const f32 floorHeight = context.mTileSpatialGrid.getFloorHeight();
    // Helper function for checking if tile is unowned and therefore external (TODO: Owned could still be external in the garden)
    auto isTileExternal = [&](i32v2 outerPos, i32 floorIndex) -> bool {
        if (outerPos.x < 0 || outerPos.y < 0 || outerPos.x >= dims.x || outerPos.y >= dims.y) {
            return true;
        }
        return context.ownerArray[outerPos.x + outerPos.y * dims.x + floorIndex * floorSize] == INVALID_ROOM_ID;
    };

    for (auto&& room : context.rooms) {
        if (room.edgeWalk.size() <= 4) { // Fairly arbitrary, this could be larger
            continue;
        }
        // Edge walk is guaranteed to always begin on a southern edge
        Cartesian currentEdgeNormal = Cartesian::SOUTH;
        TileIndex prevInteriorTileIndex = room.edgeWalk[0];
        i32v2 prevOffset2D = context.mTileSpatialGrid.getTileXYOffset(prevInteriorTileIndex);
        ExteriorWallRun* currentExteriorWallRun = nullptr;
        if (isTileExternal(prevOffset2D + CARTESIAN_NORMALS_2D[e_cast(currentEdgeNormal)], room.floorIndex)) {
            exteriorWallRuns.emplace_back(ExteriorWallRun{ prevInteriorTileIndex + room.floorIndex * floorSize, 1, currentEdgeNormal});
            currentExteriorWallRun = &exteriorWallRuns.back();
        }
        if (visLog && currentExteriorWallRun) {
            const f32v3 outerPos = f32v3(prevOffset2D.x + CARTESIAN_NORMALS_2D[e_cast(currentEdgeNormal)].x, prevOffset2D.y + CARTESIAN_NORMALS_2D[e_cast(currentEdgeNormal)].y, room.floorIndex * floorHeight);
            visLog->addWireQuad(outerPos + f32v3(0.1f, 0.1f, 0.0f), f32v2(0.8f), color::Green);
        }

        // We need to build a list of exterior edges by walking the edge and checking for unowned tiles (TODO: Exterior could still be owned)
        for (size_t i = 1; i < room.edgeWalk.size(); ++i) {
            // Current edge walk tile
            const TileIndex currentInteriorTileIndex = room.edgeWalk[i];
            const i32v2 currentOffset2D = context.mTileSpatialGrid.getTileXYOffset(currentInteriorTileIndex);

            // Detect if we just turned a corner
            bool didTurnCorner = false;
            switch (currentEdgeNormal) {
                case Cartesian::WEST:
                case Cartesian::EAST:
                    if (currentOffset2D.x != prevOffset2D.x) {
                        didTurnCorner = true;
                        if (currentOffset2D.x > prevOffset2D.x) {
                            currentEdgeNormal = Cartesian::SOUTH;
                        }
                        else {
                            currentEdgeNormal = Cartesian::NORTH;
                        }
                    }
                    break;
                case Cartesian::SOUTH:
                case Cartesian::NORTH:
                    if (currentOffset2D.y != prevOffset2D.y) {
                        didTurnCorner = true;
                        if (currentOffset2D.y > prevOffset2D.y) {
                            currentEdgeNormal = Cartesian::EAST;
                        }
                        else {
                            currentEdgeNormal = Cartesian::WEST;
                        }
                    }
                    break;
                default:
                    assert(false);
                    break;

            }
            if (didTurnCorner) {
                currentExteriorWallRun = nullptr; // End previous edge
                i32v2 newPrevExteriorOffset = prevOffset2D + CARTESIAN_NORMALS_2D[e_cast(currentEdgeNormal)];
                if (isTileExternal(newPrevExteriorOffset, room.floorIndex)) {
                    // Build new exterior edge starting at corner
                    exteriorWallRuns.emplace_back(ExteriorWallRun{ prevInteriorTileIndex + room.floorIndex * floorSize, 1, currentEdgeNormal });
                    currentExteriorWallRun = &exteriorWallRuns.back();
                    if (visLog) {
                        const f32v3 outerPos = f32v3(newPrevExteriorOffset.x, newPrevExteriorOffset.y, room.floorIndex * floorHeight);
                        visLog->addWireQuad(outerPos + f32v3(0.1f, 0.1f, 0.0f), f32v2(0.8f), color::Orange);
                    }
                }
            }

            const i32v2 outerOffset2D = currentOffset2D + CARTESIAN_NORMALS_2D[e_cast(currentEdgeNormal)];
            if (isTileExternal(outerOffset2D, room.floorIndex)) {
                if (currentExteriorWallRun) {
                    ++currentExteriorWallRun->length;
                    assert(currentEdgeNormal == currentExteriorWallRun->dir);
                }
                else {
                    // New edge
                    exteriorWallRuns.emplace_back(ExteriorWallRun{ currentInteriorTileIndex + room.floorIndex * floorSize, 1, currentEdgeNormal });
                    currentExteriorWallRun = &exteriorWallRuns.back();
                }
            }
            else {
                // End previous edge
                currentExteriorWallRun = nullptr;
                if (visLog && currentExteriorWallRun) {
                    const f32v3 outerPos = f32v3(outerOffset2D.x, outerOffset2D.y, room.floorIndex * floorHeight);
                    visLog->addFilledQuad(outerPos + f32v3(0.1f, 0.1f, 0.0f), f32v2(0.8f), color::Red);
                }
            }

            if (visLog && currentExteriorWallRun) {
                const f32v3 outerPos = f32v3(outerOffset2D.x, outerOffset2D.y, room.floorIndex * floorHeight);
                visLog->addWireQuad(outerPos + f32v3(0.1f, 0.1f, 0.0f), f32v2(0.8f), color::Magenta);
            }

            prevOffset2D = currentOffset2D;
            prevInteriorTileIndex = currentInteriorTileIndex;
        }
        // Final tile is shared with start tile and is guarenteed to be a west cartesian
        const TileIndex finalTileIndex = room.edgeWalk[0];
        const i32v2 finalOffset2D = context.mTileSpatialGrid.getTileXYOffset(finalTileIndex);
        const i32v2 finalExternalOffset2D = finalOffset2D + CARTESIAN_NORMALS_2D[e_cast(Cartesian::WEST)];
        if (isTileExternal(finalExternalOffset2D, room.floorIndex)) {
            if (currentExteriorWallRun && currentExteriorWallRun->dir == Cartesian::WEST) {
                ++currentExteriorWallRun->length;
            }
            else {
                exteriorWallRuns.emplace_back(ExteriorWallRun{ finalTileIndex + room.floorIndex * floorSize, 1, Cartesian::WEST });
            }
            if (visLog) {
                const f32v3 outerPos = f32v3(finalExternalOffset2D.x, finalExternalOffset2D.y, room.floorIndex * floorHeight);
                visLog->addWireQuad(outerPos + f32v3(0.1f, 0.1f, 0.0f), f32v2(0.8f), color::Red);
            }
        }
    }

    if (visLog) {
        for (ExteriorWallRun& wallRun : exteriorWallRuns) {
            const i32v2 edgeOffset = CARTESIAN_TANGENTS_CCW[e_cast(wallRun.dir)] * (i32)wallRun.length;
            i32v3 startOffset = context.mTileSpatialGrid.getTileXYZOffset(wallRun.start) + CARTESIAN_TILE_EDGE_WALK_CCW_POSITION_OFFSETS_3D[e_cast(wallRun.dir)];
            startOffset.z *= floorHeight;
            visLog->addLineBetweenPoints(startOffset, startOffset + i32v3(edgeOffset.x, edgeOffset.y, 0), CARTESIAN_COLORS[e_cast(wallRun.dir)]);
        }
    }

    // Compress memory
    exteriorWallRuns.shrink_to_fit();
}

void BuildingBlueprintGenerator::placeWindows(BuildingBlueprintGenerationContext& context, VisualLog* visLog) {

    if (visLog) visLog->nextStep("Place Windows");

    
    const i32v2& floorDims = context.mTileSpatialGrid.getDims2D();
    const f32 floorHeight = context.mTileSpatialGrid.getFloorHeight();
    const i32 INDEX_OFFSETS[4] = {
        1, //South
        -floorDims.x, //West
        floorDims.x, //East
        -1 //North
    };

    for (ExteriorWallRun& wallRun : context.exteriorWallRuns) {
        if (wallRun.length > 2 && wallRun.length <= MAX_EXTERIOR_WALL_RUN_LENGTH) {
            const ui32 permutation = context.randomGen->getRandomUint() % sPossibleWindowPermutations[wallRun.length].size();
            const std::vector<bool>& windowPlacements = sPossibleWindowPermutations[wallRun.length][permutation];
            assert(windowPlacements.size() == wallRun.length);
            TileIndex index = wallRun.start;
            for (ui32 i = 0; i < wallRun.length; ++i) {
                if (windowPlacements[i]) {
                    TileWall wall = context.walls.getWallAtTile(index, wallRun.dir);
                    // Don't ever replace doors
                    if (!wall.isDoor) {
                        context.walls.setWallAtTile(index, TileWall{ context.tileIDs[e_cast(BlueprintTileType::WINDOW)], false /*isDoor*/ }, wallRun.dir);
                        if (visLog) {
                            i32v3 offset = context.mTileSpatialGrid.getTileXYZOffset(index);
                            offset.z *= floorHeight;
                            visLog->addFilledQuad(offset, f32v2(1.0f), color::Aqua);
                        }
                    }
                    else if (visLog) {
                        i32v3 offset = context.mTileSpatialGrid.getTileXYZOffset(index);
                        offset.z *= floorHeight;
                        visLog->addFilledQuad(offset, f32v2(1.0f), color::Red);
                    }
                }
                if (visLog) {
                    i32v3 offset = context.mTileSpatialGrid.getTileXYZOffset(index);
                    offset.z *= floorHeight;
                    visLog->addWireQuad(offset, f32v2(1.0f), color::Red);
                }
                index += INDEX_OFFSETS[e_cast(wallRun.dir)];
            }
        }
    }
}

void BuildingBlueprintGenerator::postProcessBlueprint(BuildingBlueprintGenerationContext& context) {
    // Tally required items
    std::map<ItemID, ui32> requiredItems;
    TileRepository& tileRepo = TileRepository::get();
    
    context.tileRecipes[e_cast(BlueprintTileType::NONE)] = nullptr;
    context.tileRecipes[e_cast(BlueprintTileType::FLOOR)] = &tileRepo.getRecipeForTile(context.tileIDs[e_cast(BlueprintTileType::FLOOR)]);
    context.tileRecipes[e_cast(BlueprintTileType::DOOR)] = &tileRepo.getRecipeForTile(context.tileIDs[e_cast(BlueprintTileType::DOOR)]);
    context.tileRecipes[e_cast(BlueprintTileType::WALL)] = &tileRepo.getRecipeForTile(context.tileIDs[e_cast(BlueprintTileType::WALL)]);
    context.tileRecipes[e_cast(BlueprintTileType::WINDOW)] = &tileRepo.getRecipeForTile(context.tileIDs[e_cast(BlueprintTileType::WINDOW)]);
    context.tileRecipes[e_cast(BlueprintTileType::STAIRS)] = &tileRepo.getRecipeForTile(context.tileIDs[e_cast(BlueprintTileType::STAIRS)]);
    context.tileRecipes[e_cast(BlueprintTileType::STAIRS_FLAT)] = &tileRepo.getRecipeForTile(context.tileIDs[e_cast(BlueprintTileType::STAIRS_FLAT)]);
    context.tileRecipes[e_cast(BlueprintTileType::AIR)] = nullptr;
    static_assert(e_cast(BlueprintTileType::TYPES) == 8);

    auto addRequiredItems = [&](BlueprintTileType type) {
        const Recipe& recipe = *context.tileRecipes[e_cast(type)];
        const ui16 itemCount = recipe.mItemCount;
        for (ui32 r = 0; r < recipe.mItemCount; ++r) {
            const ItemStack stack = recipe.mItems[r];
            auto&& it = requiredItems.find(stack.id);
            if (it == requiredItems.end()) {
                requiredItems[stack.id] = stack.quantity;
            }
            else {
                it->second += stack.quantity;
            }
        }
    };

    // Compute required items, count tiles, count walls
    for (TileIndex tileIndex = 0; tileIndex < (TileIndex)context.tiles.size(); ++tileIndex) {
        BlueprintTileType type = context.tiles[tileIndex];
        TileWall walls[2];
        context.walls.getSouthAndWestWallsAtTile(walls, tileIndex);
        for (int i = 0; i < 2; ++i) {
            if (walls[i].isValid()) [[unlikely]] {
                ++context.totalWalls;
                if (walls[i].isDoor) {
                    addRequiredItems(BlueprintTileType::DOOR);
                }
                else {
                    addRequiredItems(BlueprintTileType::WALL);
                }
            }
        }
        switch (type) {
            case BlueprintTileType::NONE:
            case BlueprintTileType::AIR:
                break;
            case BlueprintTileType::STAIRS:
            case BlueprintTileType::STAIRS_FLAT:
                addRequiredItems(type);
                break;
            case BlueprintTileType::WINDOW:
            case BlueprintTileType::FLOOR:{
                addRequiredItems(type);
                ++context.totalTiles;
                break;
            }
            case BlueprintTileType::DOOR:
            case BlueprintTileType::WALL:
            case BlueprintTileType::TYPES:
            default:
                assert(false);
                break;
        }
    }
    static_assert(e_cast(BlueprintTileType::TYPES) == 8);

    context.requiredItemsToBuild.reserve(requiredItems.size());
    for (auto&& it : requiredItems) {
        context.requiredItemsToBuild.emplace_back(it.first, it.second);
    }
}

BuildingBlueprintPtr BuildingBlueprintGenerator::finalizeBlueprint(BuildingBlueprintGenerationContext& context) {
    BuildingBlueprintPtr bp = std::make_unique<BuildingBlueprint>();
    bp->floorCount = context.floorCount;
    bp->floorHeight = context.mTileSpatialGrid.getFloorHeight();
    bp->worldPosRootDTile = context.rootPosDTileCoord;
    bp->dimsDTile = context.dimsDTile;
    // Items
    bp->itemCompositionCount = context.requiredItemsToBuild.size();
    bp->itemComposition = std::make_unique<ItemStack[]>(bp->itemCompositionCount);
    memcpy(bp->itemComposition.get(), context.requiredItemsToBuild.data(), sizeof(ItemStack) * bp->itemCompositionCount); // TODO: if we used vector or ptr we could just std::move...

    bp->tileTargetCount = context.totalTiles;
    bp->tileTargets = std::make_unique<BuildingBlueprintTileTarget[]>(bp->tileTargetCount);

    bp->wallTargetCount = context.totalWalls;
    bp->wallTargets = std::make_unique<BuildingBlueprintWallTarget[]>(bp->wallTargetCount);

    i32 wallN = 0;
    i32 tileN = 0;
    // TODO: FINISH
    for (TileIndex tileIndex = 0; tileIndex < (TileIndex)context.tiles.size(); ++tileIndex) {
        // TODO: Bitindex
        const BlueprintTileType type = context.tiles[tileIndex];

        TileWall walls[2];
        context.walls.getSouthAndWestWallsAtTile(walls, tileIndex);
        if (walls[0].isValid()) {
            bp->wallTargets[wallN++] = BuildingBlueprintWallTarget{ tileIndex, walls[0].wallID, Cartesian::SOUTH };
        }
        if (walls[1].isValid()) {
            bp->wallTargets[wallN++] = BuildingBlueprintWallTarget{ tileIndex, walls[1].wallID, Cartesian::WEST };
        }
        // Copy walls
        if (type != BlueprintTileType::NONE) {

            // Stairs are processed below
            if (type != BlueprintTileType::STAIRS) {
                const TileID tileId = context.tileIDs[e_cast(type)];
                if (tileId != TILE_ID_NONE) {
                    bp->tileTargets[tileN++] = BuildingBlueprintTileTarget{ tileIndex, tileId };
                }
            }
        }
    }
    assert(tileN == bp->tileTargetCount);
    assert(wallN == bp->wallTargetCount);
    // Copy room data
    //newBuilding->mRooms = std::move(bp.rooms);

    // Set stairs tiles
    bp->stairPieceCount = 0;
    for (auto& stairsVec : context.stairs) {
        bp->stairPieceCount += stairsVec.size();
    }


    bp->stairPieces = std::make_unique<StairPiece[]>(bp->stairPieceCount);
    ui32 startIndex = 0;
    for (auto& stairsVec : context.stairs) {
        memcpy(&bp->stairPieces[startIndex], stairsVec.data(), sizeof(StairPiece) * stairsVec.size());
        startIndex += stairsVec.size();
    }

    bp->stairsTileID = context.tileIDs[e_cast(BlueprintTileType::STAIRS)];
    bp->stairsFlatTileID = context.tileIDs[e_cast(BlueprintTileType::STAIRS_FLAT)];
    bp->desc = context.desc;

    bp->ownedDTiles = std::move(context.ownedDTiles);
    return bp;
}
