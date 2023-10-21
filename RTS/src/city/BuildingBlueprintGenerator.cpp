#include "stdafx.h"
#include "BuildingBlueprintGenerator.h"
#include "BuildingDescriptionRepository.h"
#include "BuildingBlueprint.h"

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

BuildingBlueprintId BuildingBlueprintGenerator::sCurrentId = 0;

void renderBlueprintDebugVislog(BuildingBlueprint& bp, VisualLog& visLog, color4* inputColor/* = nullptr*/) {
    visLog.nextStep("Final Floorplan");
    // Render the AABB of the floor plan
    constexpr f32 EPSILON = 0.001f;

    // Entire AABB
    const i32AABB3& aabb = bp.mTileSpatialGrid.getAABB();
    const i32 floorHeight = bp.mTileSpatialGrid.getFloorHeight();
    visLog.addWireQuad(f32v3(0.0f, 0.0f, 0.0f), aabb.dims, inputColor ? *inputColor : color4(0.7f, 0.4f, 0.0f));

    // AABBS first
    int i = 0;
    for (auto&& node : bp.rooms) {

        const color4& color = ROOM_COLORS[i % MAX_ROOM_COLORS];
        visLog.addWireQuad(f32v3(node.aabb.x - aabb.x, node.aabb.y - aabb.y, node.floorIndex * floorHeight), node.aabb.dims, color4(color.r, color.g, color.b, 255u));
        // Draw parent line
        if (node.parentRoom != INVALID_ROOM_ID) {
            RoomNode& parent = bp.rooms[node.parentRoom];
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
        node.roomDef->nameToken.toString(buf, nullptr);
        visLog.addText(buf, f32v3(node.offsetFromZero.x + 0.5f, node.offsetFromZero.y + 0.5f, node.floorIndex * floorHeight), 0.25f, f32v2(0.0f, 0.5f), color4(color.r, color.g, color.b, 255u));
        ++i;
    }

    // Render all the tiles
    for (int z = 0; z < bp.floorCount; ++z) {
        const i32 floorIndex = z * aabb.dims.x * aabb.dims.y;
        for (int y = 0; y < aabb.dims.y; ++y) {
            for (int x = 0; x < aabb.dims.x; ++x) {
                const TileIndex index = floorIndex + y * aabb.dims.x + x;
                RoomNodeID id = bp.ownerArray[index];
                if (id != INVALID_ROOM_ID) {
                    const RoomNode& room = bp.rooms[id];
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

BuildingBlueprintGenerator::BuildingBlueprintGenerator(BuildingDescriptionRepository& buildingRepo, CityBuilder& cityBuilder) :
    mBuildingRepo(buildingRepo),
    mCityBuilder(cityBuilder)
{
    generatePossibleWindowPermutations();
}

std::unique_ptr<BuildingBlueprint> BuildingBlueprintGenerator::generateBlueprintAsyncThenSendToBuilder(World& world, const BuildingDef& desc, float sizeAlpha, Cartesian entrySide, i32v2 plotSize, const i32v3& worldPosRoot, entt::entity ownerEntity, BuildingBlueprintFlags flags)
{
    assert(desc.publicRoomCountRange.y != 0.0f);

    std::unique_ptr<BuildingBlueprint> bp = std::make_unique<BuildingBlueprint>(world, desc, sizeAlpha, entrySide, plotSize, worldPosRoot, ownerEntity, flags);
    assert(plotSize.x > 2 && plotSize.y > 2);
    BuildingBlueprint* bPtr = bp.get();
    mGeneratingBuildings.insert(bPtr);
    
    Services::Threadpool::ref().addTask([this, &world, bPtr, &desc, sizeAlpha, entrySide, plotSize, worldPosRoot, ownerEntity, flags](ThreadPoolWorkerData* workerData) {
        PROFILE_FUNCTION("Generate blueprint async");
        std::unique_ptr<BuildingBlueprint> newBP = tryGenerateBlueprintSynchronous(world, mBuildingRepo, desc, sizeAlpha, entrySide, plotSize, worldPosRoot, ownerEntity, flags);
        if (newBP) {
            // TODO: If we destroy the original we are fucked
            // Copy the result to the output bp
            *bPtr = std::move(*newBP);
        }
        else {
            bPtr->flags.setBit(BuildingBlueprintFlags::BLUEPRINT_FLAG_FAILED_TO_GENERATE);
        }

    }, [&, bPtr]() {
        // Main thread
        mGeneratingBuildings.erase(bPtr);
        bPtr->isGenerating = false;
        mCityBuilder.addBlueprintToBuildAndPreprocess(bPtr);
    });
    return bp;
}

std::unique_ptr<BuildingBlueprint> BuildingBlueprintGenerator::tryGenerateBlueprintSynchronous(World& world, BuildingDescriptionRepository& buildingRepo, const BuildingDef& desc, float sizeAlpha, Cartesian entrySide, i32v2 plotSize, const i32v3& worldPosRoot, entt::entity ownerEntity, BuildingBlueprintFlags flags) {
    PROFILE_FUNCTION();
    BuildingBlueprintId id = getNextBuildingID(); // TODO: Move this to game thread only so we dont need to lock?
    constexpr ui32 maxFailCount = 5;
    ui32 failCount = 0;
    do {
        std::unique_ptr<BuildingBlueprint> bp = std::make_unique<BuildingBlueprint>(world, desc, sizeAlpha, entrySide, plotSize, worldPosRoot, ownerEntity, flags);
        assert(plotSize.x > 2 && plotSize.y > 2);
        bp->id = id;
        if (tryGenerateBlueprintInternal(bp.get(), buildingRepo)) {
            if (failCount > 0) {
                LOG_DEBUG("Finished building with fail count {}", failCount);
            }
            return bp;
        }
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

bool BuildingBlueprintGenerator::tryGenerateBlueprintInternal(BuildingBlueprint* bPtr, BuildingDescriptionRepository& buildingRepo) {

    VisualLog* visLog = VisualLogger::tryGetNewVisualLog("Blueprint");
    if (visLog) {
        visLog->setRootPos(f32v3(bPtr->mTileSpatialGrid.getWorldPos3D()));
    }

    // Room Graph
    addPublicRoomsToGraph(*bPtr);
    assignPublicRooms(*bPtr);
    addPrivateRoomsToGraph(*bPtr);

    // Assign roomDefs
    for (auto& room : bPtr->rooms) {
        room.roomDef = &buildingRepo.getRoomDefFromID(room.roomDefId);
    }

    // Rooms
    initRooms(*bPtr, buildingRepo);
    placeRooms(*bPtr, visLog);
    allocateTileData(*bPtr);
    expandRooms(*bPtr, visLog);
    roomCleanup(*bPtr, visLog);
    computeRoomAABBs(*bPtr, visLog);
    if (!validateRoomsArentEmpty(*bPtr, visLog)) {
        if (visLog) {
            visLog->nextStep("INVALID GENERATION - EMPTY ROOM");
            visLog->finish();
        }
        return false;
    }

    // Walls
    placeWalls(*bPtr, visLog);

    // Room edge info
    buildRoomInteriorEdges(*bPtr, visLog);

    // Doors
    placeDoors(*bPtr, visLog);

    // Stairs
    placeStairs(*bPtr, visLog);

    // Windows + facade details
    buildExteriorWallRuns(*bPtr, visLog);
    placeWindows(*bPtr, visLog);

    // Furniture

    // Flooring

    // Tally final item requirements
    postProcessBlueprint(*bPtr);

    if (visLog) {
        // Draw the entire room graph
        renderBlueprintDebugVislog(*bPtr, *visLog, nullptr);
        visLog->finish();
    }

    return true;
}

void BuildingBlueprintGenerator::addPublicRoomsToGraph(BuildingBlueprint& bp) {
    // Generate public room structure using grammar
    const i32 publicRoomCount = bp.desc->publicRoomCountRange.y <= bp.desc->publicRoomCountRange.x ?
        bp.desc->publicRoomCountRange.x : Random::xorshf96() % (bp.desc->publicRoomCountRange.y - bp.desc->publicRoomCountRange.x) + bp.desc->publicRoomCountRange.x;
    assert(publicRoomCount); // Must have at least one public room
    bp.rooms.resize(publicRoomCount);
    bp.desc->publicGrammar.buildRoomGraph(bp.rooms);
}

void BuildingBlueprintGenerator::assignPublicRooms(BuildingBlueprint& bp)
{
    assert(bp.desc->publicRooms.size());
    ui8v2 countLookup[255]; // (current, max)
    const i32 publicRoomCount = (i32)bp.desc->publicRooms.size();
    i32 availablePublicRooms = 0;

    { // Pre-pass set up count lookup
        i32 roomIndex = 0;
        for (roomIndex = 0; roomIndex < publicRoomCount; ++roomIndex) {
            const PossibleRoom& room = bp.desc->publicRooms[roomIndex];
            countLookup[roomIndex].x = 0;
            countLookup[roomIndex].y = room.countRange.y;
            availablePublicRooms += room.countRange.y;
        }
    }
    assert(availablePublicRooms >= publicRoomCount);

    {// Generate rooms in order of priority while breadth first walking the tree
        i32 roomIndex = 0;
        for (auto&& node : bp.rooms) {
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
            node.roomDefId = bp.desc->publicRooms[roomIndex++].id;
            
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

void BuildingBlueprintGenerator::addPrivateRoomsToGraph(BuildingBlueprint& bp) {
    const size_t numPublicRooms = bp.rooms.size();
    ui8v2 countLookup[255]; // (current, max)
    assert(numPublicRooms);
    i32 privateRoomCount = round(bp.sizeAlpha * (bp.desc->privateRoomCountRange.y - bp.desc->privateRoomCountRange.x) + bp.desc->privateRoomCountRange.x);
    if (!privateRoomCount) {
        return;
    }

    i32 availablePrivateRooms = 0;

    { // Pre-pass set up count lookup
        i32 roomIndex = 0;
        for (roomIndex = 0; roomIndex < bp.desc->privateRooms.size(); ++roomIndex) {
            const PossibleRoom& room = bp.desc->privateRooms[roomIndex];
            ui8 roomCount = vmath::roundApprox(vmath::lerp((f32)room.countRange.x, (f32)room.countRange.y, bp.sizeAlpha));
            countLookup[roomIndex].x = 0;
            countLookup[roomIndex].y = roomCount;
            availablePrivateRooms += roomCount;
        }
    }
    if (availablePrivateRooms < privateRoomCount) {
        privateRoomCount = availablePrivateRooms;
    }

    bp.rooms.reserve(bp.rooms.size() + privateRoomCount);
    int failCount = 0;
    int publicIndex = Random::xorshf96() % numPublicRooms; // Random start room to test
    int privateIndex = 0;
    for (size_t i = 0; i < privateRoomCount; ++i) {
        RoomNode& publicRoom = bp.rooms[publicIndex];
        if (publicRoom.numChildren < MAX_CHILD_ROOMS && countLookup[privateIndex].x < countLookup[privateIndex].y) {
            // We can fit a private room here
            // Next node index is our child
            publicRoom.childRooms[publicRoom.numChildren++] = (RoomNodeID)bp.rooms.size();
            // Append the room
            RoomNode privateRoom;
            privateRoom.roomDefId = bp.desc->privateRooms[privateIndex].id;
            privateRoom.parentRoom = publicIndex;
            privateRoom.isPrivate = true;
            bp.rooms.emplace_back(std::move(privateRoom));
            // Limit our private count
            ++countLookup[privateIndex++].x;
            // Wrap
            if (privateIndex >= bp.desc->privateRooms.size()) {
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

void placeChildrenRecursive(BuildingBlueprint& bp, RoomNode* node, f32 availableWidthSpan, i32 maxXOffsetPerLayer, i32v2 currentOffset, const i32v2& dims2d, VisualLog* visLog) {
    if (node->numChildren == 0) {
        return;
    }
    assert(currentOffset.x < 10000 && currentOffset.y < 10000);
    std::vector<RoomNode>& nodes = bp.rooms;
    const f32 floorHeight = bp.mTileSpatialGrid.getFloorHeight();

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
        if (node->roomDef->stairsChance && child.roomDef->canStairsConnect && child.desiredWidth >= 3) {
            if (Random::getCachedRandomf() > node->roomDef->stairsChance) {
                didCreateStairs = true;
                node->hasStairs = true;

                child.connectedToParentWithStairs = true;
                child.offsetFromZero = currentOffset;
                child.floorIndex = node->floorIndex + 1;
                // Force size to match
                child.desiredWidth = node->desiredWidth;
                child.desiredSize = node->desiredSize;
                if (child.floorIndex == bp.floorCount) {
                    ++bp.floorCount;
                }
                // Visual log
                if (visLog) {
                    const color4& color = ROOM_COLORS[node->childRooms[i] % MAX_ROOM_COLORS];
                    const f32v3 childPos(child.offsetFromZero.x, child.offsetFromZero.y, child.floorIndex * floorHeight);
                    visLog->addWireQuad(childPos, f32v2(1.0f), color);
                    const f32v3 parentPos(node->offsetFromZero.x, node->offsetFromZero.y, node->floorIndex * floorHeight);
                    visLog->addLineBetweenPoints(childPos, parentPos, color);

                    char buf[64];
                    child.roomDef->nameToken.toString(buf, nullptr);
                    visLog->addText(buf, f32v3(childPos.x + 0.5f, childPos.y + 0.5f, child.floorIndex * floorHeight), 0.25f, f32v2(0.0f, 0.5f), color4(color.r, color.g, color.b, 255u));
                }
                placeChildrenRecursive(bp, &child, availableWidthSpan, maxXOffsetPerLayer, child.offsetFromZero, dims2d, visLog);
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
                child.roomDef->nameToken.toString(buf, nullptr);
                visLog->addText(buf, f32v3(childPos.x + 0.5f, childPos.y + 0.5f, child.floorIndex * floorHeight), 0.25f, f32v2(0.0f, 0.5f), color4(color.r, color.g, color.b, 255u));
            }
            assert(child.offsetFromZero.x < 10000 && child.offsetFromZero.y < 10000);
            placeChildrenRecursive(bp, &child, childWidthSpan, maxXOffsetPerLayer, child.offsetFromZero, dims2d, visLog);
            currentOffset.y += widthSegmentSize * 2;
        }
    }
}

void BuildingBlueprintGenerator::initRooms(BuildingBlueprint& bp, BuildingDescriptionRepository& buildingRepo) {
    for (size_t i = 0; i < bp.rooms.size(); ++i) {
        RoomNode& room = bp.rooms[i];
        room.id = (RoomNodeID)i;

        const RoomDef& desc = buildingRepo.getRoomDefFromID(room.roomDefId);
        room.desiredWidth = (i32)round(lerp((f32)desc.minWidth, (f32)desc.maxWidth, bp.sizeAlpha));
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

void BuildingBlueprintGenerator::placeRooms(BuildingBlueprint& bp, VisualLog* visLog) {
    if (visLog) visLog->nextStep("Place rooms");
    // Breadth first search room placement
    RoomNode* root = &bp.rooms[0];
    i32 maximumDepth = getMaximumDepthRecursive(bp.rooms, root);
    const i32v3& dims3D = bp.mTileSpatialGrid.getDims();

    // Determine which dims to use for cartesian
    i32v2 dims;
    switch (bp.entrySide) {
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
        root->roomDef->nameToken.toString(buf, nullptr);
        visLog->addText(buf, f32v3(root->offsetFromZero.x + 0.5f, root->offsetFromZero.y + 0.5f, root->floorIndex * bp.mTileSpatialGrid.getFloorHeight()), 0.25f, f32v2(0.0f, 0.5f), COLOR_WHITE);
    }
    placeChildrenRecursive(bp, root, availableWidthSpan, maxDepthOffsetPerLayer, root->offsetFromZero, dims, visLog);

    // Rotate all coordinates around for Cartesian direction
    // Left is the base case so do nothing for that
    switch (bp.entrySide) {
        case Cartesian::SOUTH:
            for (auto&& room : bp.rooms) {
                i32 tmp = room.offsetFromZero.x;
                room.offsetFromZero.x = room.offsetFromZero.y;
                room.offsetFromZero.y = dims3D.y - tmp - 1;
            }
            break;
        case Cartesian::EAST:
            for (auto&& room : bp.rooms) {
                room.offsetFromZero.x = dims3D.x - room.offsetFromZero.x - 1;
            }
            break;
        case Cartesian::NORTH:
            for (auto&& room : bp.rooms) {
                std::swap(room.offsetFromZero.x, room.offsetFromZero.y);
                room.offsetFromZero.x = dims3D.x - room.offsetFromZero.x - 1;
            }
            break;
    }

    // Clamp positions to be withing facade
    for (auto&& room : bp.rooms) {
        room.offsetFromZero.x = vmath::clamp((i32)room.offsetFromZero.x, (i32)1u, (i32)dims3D.x);
        room.offsetFromZero.y = vmath::clamp((i32)room.offsetFromZero.y, (i32)1u, (i32)dims3D.y);
        assert(room.offsetFromZero.x < 10000 && room.offsetFromZero.y < 10000);
    }

    // Spread rooms apart based on circular collision
    constexpr f32 FORCE_MULT = 0.5f;
    for (int iter = 0; iter < 3; ++iter) {
        for (size_t i = 0; i < bp.rooms.size() - 1; ++i) {
            RoomNode& room1 = bp.rooms[i];
            for (size_t j = i + 1; j < bp.rooms.size(); ++j) {
                RoomNode& room2 = bp.rooms[j];
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
        for (size_t i = 0; i < bp.rooms.size() - 1; ++i) {
            RoomNode& room1 = bp.rooms[i];
            for (size_t j = i + 1; j < bp.rooms.size(); ++j) {
                RoomNode& room2 = bp.rooms[j];
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

i32v2 WALL_EXPAND_OFFSETS[4] = {
    { 0, -1}, // SOUTH
    {-1,  0}, // WEST
    { 1,  0}, // EAST
    { 0,  1}  // NORTH
};

i32v2 WALL_ITERATE_OFFSETS[4] = {
    { 1,  1}, // SOUTH
    { 0,  1}, // WEST
    { 0,  1}, // EAST
    { 1,  0}  // NORTH
};


void expandWall(Cartesian wallDir, BuildingBlueprint& bp, RoomNode& room, VisualLog* visLog) {

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
        const i32 index = getIndex2DAtPos(iterPos, bp.mTileSpatialGrid.getDims(), room.floorIndex);
        RoomNodeID ownerId = bp.ownerArray[index];
        assert(ownerId == INVALID_ROOM_ID);
        //if (ownerId != INVALID_ROOM_ID) {
        //    RoomNode& ownerRoom = bp.rooms[ownerId];
        //    // TODO: Can we optimize this so we don't run it every time?
        //}
        bp.ownerArray[index] = room.id;
        bp.tiles[index] = BlueprintTileType::FLOOR;

        if (visLog) {
            const color4& color = ROOM_COLORS[room.id % MAX_ROOM_COLORS];
            const f32v3 rootPos(iterPos.x, iterPos.y, room.floorIndex * bp.mTileSpatialGrid.getFloorHeight());
            visLog->addWireQuad(rootPos + f32v3(0.1f, 0.1f, 0.0f), f32v2(0.8f), color);
        }
        // Step
        iterPos += iterateOffset;
    }
    room.size += length;
}

// Only fills gaps and will not overwrite any existing walls
void expandWallGapsOnly(Cartesian wallDir, BuildingBlueprint& bp, RoomNode& room, VisualLog* visLog) {
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
        const i32 index = getIndex2DAtPos(iterPos, bp.mTileSpatialGrid.getDims(), room.floorIndex);
        RoomNodeID ownerId = bp.ownerArray[index];
        if (ownerId == INVALID_ROOM_ID) {
            ++tilesAdded;
            bp.ownerArray[index] = room.id;
            bp.tiles[index] = BlueprintTileType::FLOOR;
            // Visual log
            if (visLog) {
                visLog->addFilledQuad(f32v3(iterPos.x + 0.1f, iterPos.y + 0.1f, room.floorIndex * bp.mTileSpatialGrid.getFloorHeight()), f32v2(0.8f), ROOM_COLORS[room.id % MAX_ROOM_COLORS]);
            }
        }
        // Step
        iterPos += iterateOffset;
    }

    // TODO: Only along actual expansion tiles
    // TODO: Decrement size when we push into another room
    room.size += tilesAdded;
}

bool expandRoomSquare(BuildingBlueprint& bp, RoomNode& room, VisualLog* visLog) {

    bool didExpand = false;
    const i32v3& dims = bp.mTileSpatialGrid.getDims();
    const f32 floorHeight = bp.mTileSpatialGrid.getFloorHeight();
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
                expandWall(wallDir, bp, room, visLog);
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

bool expandRoomGaps(BuildingBlueprint& bp, RoomNode& room, VisualLog* visLog) {

    bool didExpand = false;

    const i32v3& dims = bp.mTileSpatialGrid.getDims();
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
                    RoomNodeID ownerId = bp.ownerArray[index];
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
                    expandWallGapsOnly(wallDir, bp, room, visLog);
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

void BuildingBlueprintGenerator::placeWalls(BuildingBlueprint& bp, VisualLog* visLog) {
    if (visLog) visLog->nextStep("Place walls");
    const i32v3& dims = bp.mTileSpatialGrid.getDims();
    const f32 floorHeight = bp.mTileSpatialGrid.getFloorHeight();
    // First place main segments
    for (i32 z = 0; z < bp.floorCount; ++z) {
        for (i32 y = 0; y < dims.y; ++y) {
            for (i32 x = 0; x < dims.x; ++x) {
                const i32 index = getIndex2DAtPos(x, y, dims, z);
                RoomNodeID roomId = bp.ownerArray[index];
                if (roomId == INVALID_ROOM_ID) {
                    continue;
                }

                bool didSetWall = false;
                TileWall walls[4];
                // South
                if (y == 0 || bp.ownerArray[getIndex2DAtPos(x, y - 1, dims, z)] != roomId) {
                    walls[e_cast(Cartesian::SOUTH)].wallID = bp.tileIDs[e_cast(BlueprintTileType::WALL)];
                    if (visLog) {
                        visLog->addLineBetweenPoints(f32v3(x, y + 0.01f, z * floorHeight), f32v3(x + 1.0f, y + 0.01f, z * floorHeight), COLOR_WHITE);
                    }
                }
                // West
                if (x == 0 || bp.ownerArray[getIndex2DAtPos(x - 1, y, dims, z)] != roomId) {
                    walls[e_cast(Cartesian::WEST)].wallID = bp.tileIDs[e_cast(BlueprintTileType::WALL)];
                    if (visLog) {
                        visLog->addLineBetweenPoints(f32v3(x + 0.01f, y, z * floorHeight), f32v3(x + 0.01f, y + 1.0f, z * floorHeight), COLOR_WHITE);
                    }
                }
                // East
                if (x == dims.x - 1 || bp.ownerArray[getIndex2DAtPos(x + 1, y, dims, z)] != roomId) {
                    walls[e_cast(Cartesian::EAST)].wallID = bp.tileIDs[e_cast(BlueprintTileType::WALL)];
                    if (visLog) {
                        visLog->addLineBetweenPoints(f32v3(x + 1.0f - 0.01f, y, z * floorHeight), f32v3(x + 1.0f - 0.01f, y + 1.0f, z * floorHeight), COLOR_WHITE);
                    }
                }
                // North
                if (y == dims.y - 1 || bp.ownerArray[getIndex2DAtPos(x, y + 1, dims, z)] != roomId) {
                    walls[e_cast(Cartesian::NORTH)].wallID = bp.tileIDs[e_cast(BlueprintTileType::WALL)];
                    if (visLog) {
                        visLog->addLineBetweenPoints(f32v3(x, y + 1.0f - 0.01f, z * floorHeight), f32v3(x + 1.0f, y + 1.0f - 0.01f, z * floorHeight), COLOR_WHITE);
                    }
                }
                bp.walls.setWallsAtTile(index, walls);
            }
        }
    }
}


void BuildingBlueprintGenerator::allocateTileData(BuildingBlueprint& bp) {
    const i32v2 dims2D = bp.mTileSpatialGrid.getDims2D();
    ui32 numTiles = dims2D.x * dims2D.y * bp.floorCount;
    // Set proper floor count into the AABB
    bp.mTileSpatialGrid.init(bp.mTileSpatialGrid.getWorldPos3D(), i32v3(dims2D.x, dims2D.y, bp.floorCount), bp.mTileSpatialGrid.getFloorHeight());
    bp.tiles.resize((size_t)bp.floorCount * dims2D.x * dims2D.y, BlueprintTileType::NONE);
    bp.walls.init(&bp.mTileSpatialGrid);
    bp.ownerArray.resize(bp.tiles.size(), INVALID_ROOM_ID);
    // Construction data
    bp.tileItemDataHandles.resize(bp.tiles.size());
    bp.tileBuildData.resize(bp.tiles.size());
}

void BuildingBlueprintGenerator::expandRooms(BuildingBlueprint& bp, VisualLog* visLog) {
    if (visLog) visLog->nextStep("Expand rooms");
    const i32v3& dims = bp.mTileSpatialGrid.getDims();

    // Init rooms
    for (size_t i = 0; i < bp.rooms.size(); ++i) {
        if (visLog) {
            f32v3 pos(bp.rooms[i].offsetFromZero.x, bp.rooms[i].offsetFromZero.y, bp.rooms[i].floorIndex * bp.mTileSpatialGrid.getFloorHeight());
            visLog->addWireQuad(pos, f32v2(1.0f), ROOM_COLORS[bp.rooms[i].id % MAX_ROOM_COLORS]);
        }
        initRoomWalls(bp, bp.rooms[i]);
    }
    // Expand one floor at a time so that second story rooms can copy their
    // lower parent layout for easier stair placement
    boost::container::static_vector<RoomNode*, 256> roomsOnThisFloorToExpand;

    for (i32 z = 0; z < bp.floorCount; ++z) {
        // Collect rooms, direct copy any rooms that are connected to parent via stairs
        roomsOnThisFloorToExpand.clear();
        for (auto&& room : bp.rooms) {
            if (room.floorIndex == z) {
                if (room.connectedToParentWithStairs) {
                    assert(z != 0);
                    // Direct copy!
                    RoomNode& parent = bp.rooms[room.parentRoom];
                    room.aabb = parent.aabb;
                    // Iterate over every tile on the floor to copy
                    // TODO: AABB iterate instead
                    for (i32 y = 0; y < dims.y; ++y) {
                        for (i32 x = 0; x < dims.x; ++x) {
                            TileIndex myIndex = getIndex2DAtPos(x, y, dims, z);
                            TileIndex parentIndex = myIndex - dims.x * dims.y;
                            if (bp.ownerArray[parentIndex] == parent.id) {
                                bp.ownerArray[myIndex] = room.id;
                                bp.tiles[myIndex] = bp.tiles[parentIndex];
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
                failCount += expandRoomSquare(bp, *room, visLog) ? 0 : 1;
            }
            if (failCount == roomsOnThisFloorToExpand.size()) {
                break;
            }
        }
        // Fill in gaps, no overwrite
        for (int iters = 0; iters < MAX_WALL_LENGTH / ITER_STEP; ++iters) {
            int failCount = 0;
            for (auto&& room : roomsOnThisFloorToExpand) {
                failCount += expandRoomGaps(bp, *room, visLog) ? 0 : 1;
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
void FixupSingleRoomPieces(BuildingBlueprint& bp, i32 x, i32 y, i32 index, VisualLog* visLog) {

    bool iterateAgain;
    const f32 floorHeight = bp.mTileSpatialGrid.getFloorHeight();
    const i32v3& dims = bp.mTileSpatialGrid.getDims();

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
        const RoomNodeID downID = bp.ownerArray[(i32)(index - dims.x)];
        ++ROOM_NODE_COUNT_CACHE[downID];
        // Left
        const RoomNodeID leftID = bp.ownerArray[(i32)(index - 1)];
        ++ROOM_NODE_COUNT_CACHE[leftID];
        // Right
        const RoomNodeID rightID = bp.ownerArray[(i32)(index + 1)];
        ++ROOM_NODE_COUNT_CACHE[rightID];
        // Top
        const RoomNodeID topID = bp.ownerArray[(i32)(index + dims.x)];
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
                    ++bp.rooms[bestId].size;
                    if (visLog) {
                        visLog->addFilledQuad(f32v3(x, y, bp.rooms[bestId].floorIndex * floorHeight), f32v2(1.0f), ROOM_COLORS[bestId % MAX_ROOM_COLORS]);
                    }
                }
                else {
                    // Clear any tile data here
                    if (visLog) {
                        visLog->addFilledQuad(f32v3(x, y, bp.rooms[myID].floorIndex * floorHeight), f32v2(1.0f), COLOR_GRAY);
                    }
                    bp.tiles[index] = BlueprintTileType::NONE;
                }

                bp.ownerArray[index] = bestId;
                --bp.rooms[myID].size;
            }
            else {
                assert(myID != INVALID_ROOM_ID);
                // Visual log
                if (visLog) {
                    visLog->addFilledQuad(f32v3(x, y, bp.rooms[myID].floorIndex * floorHeight), f32v2(1.0f), COLOR_RED);
                }
                bp.tiles[index] = BlueprintTileType::NONE;
                bp.ownerArray[index] = INVALID_ROOM_ID;
                --bp.rooms[myID].size;
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
void CellularAutomataSubStepThickenPassages(BuildingBlueprint& bp, i32 x, i32 y, i32 index, RoomNode& room) {

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

void BuildingBlueprintGenerator::roomCleanup(BuildingBlueprint& bp, VisualLog* visLog)
{
    constexpr int CELLULAR_AUTOMATA_ITERATIONS = 1;

    if (visLog) visLog->nextStep("Room cleanup");
    const i32v3& dims = bp.mTileSpatialGrid.getDims();

    for (int step = 0; step < CELLULAR_AUTOMATA_ITERATIONS; ++step) {
        for (i32 z = 0; z < bp.floorCount; ++z) {
            for (i32 y = 1; y < dims.y - 1; ++y) {
                for (i32 x = 1; x < dims.x - 1; ++x) {
                    const i32 index = getIndex2DAtPos(x, y, dims, z);
                    FixupSingleRoomPieces(bp, x, y, index, visLog);
                }
            }
        }
    }
}

void BuildingBlueprintGenerator::computeRoomAABBs(BuildingBlueprint& bp, VisualLog* visLog) {
    if (visLog) visLog->nextStep("Compute AABBs");

    std::unordered_map<RoomNodeID, i32v4 /* xspan, yspan */ > roomBoundsLookup;
    roomBoundsLookup.reserve(20);

    // Reserve
    for (size_t i = 0; i < bp.rooms.size(); ++i) {
        RoomNode& room = bp.rooms[i];
        room.tilePositions.reserve(room.size);
    }

    bp.tileItemData.reserve(bp.tiles.size() / 2);
    const i32v3& rootPos = bp.mTileSpatialGrid.getWorldPos3D();
    const i32v3& dims = bp.mTileSpatialGrid.getDims();
    const f32 floorHeight = bp.mTileSpatialGrid.getFloorHeight();
    for (TileIndex tileIndex = 0; tileIndex < (TileIndex)bp.tiles.size(); ++tileIndex) {
        // Compute bounds
        const RoomNodeID id = bp.ownerArray[tileIndex];
        if (id != INVALID_ROOM_ID) {

            const i32v2 pos = getPosAtIndex2D(tileIndex, dims);

            if (visLog) {
                const color4& color = ROOM_COLORS[id % MAX_ROOM_COLORS];
                const f32v3 rootPos(pos.x, pos.y, bp.rooms[id].floorIndex * floorHeight);
                visLog->addWireQuad(rootPos + f32v3(0.1f, 0.1f, 0.0f), f32v2(0.8f), color);
            }

            bp.rooms[id].tilePositions.emplace_back(tileIndex);
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
        RoomNode& room = bp.rooms[it.first];
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
    for (RoomNode& room : bp.rooms) {
        room.tilePositions.shrink_to_fit();
    }

}

bool BuildingBlueprintGenerator::validateRoomsArentEmpty(BuildingBlueprint& bp, VisualLog* visLog) {
    for (const RoomNode& room : bp.rooms) {
        if (room.tilePositions.empty()) {
            if (visLog) {
                char buf[64];
                room.roomDef->nameToken.toString(buf, nullptr);
                visLog->addText("INVALID ROOM: " + std::string(buf), f32v3(0.0f), 1.0f, f32v2(0.0f, 1.0f), COLOR_RED);
            }
            return false;
        }
    }
    return true;
}

void BuildingBlueprintGenerator::initRoomWalls(BuildingBlueprint& bp, RoomNode& room)
{
    const i32 index = getIndex2DAtPos(i32v2(room.offsetFromZero), bp.mTileSpatialGrid.getDims2D(), room.floorIndex);
    // Init root node
    room.size = 1;
    bp.tiles[index] = BlueprintTileType::FLOOR;
    bp.ownerArray[index] = room.id;
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

void doorBfs(std::vector<DoorBFSNode>& bfs, size_t& bfsBackIndex, BuildingBlueprint& bp, Cartesian dir, TileIndex tileIndex, RoomNode& room, const i32v2& currentPos, BitArray& visited, BitArray& isConnected, bool& canConnectToOutside, VisualLog* visLog) {
    const i32v2& directionOffset = WALL_EXPAND_OFFSETS[e_cast(dir)];
    const i32v2 nextPos = currentPos + directionOffset;
    const TileIndex nextTileIndex = getIndex2DAtPos(i32v2(nextPos), bp.mTileSpatialGrid.getDims2D(), room.floorIndex);
    const f32 floorHeight = bp.mTileSpatialGrid.getFloorHeight();
    const i32v3& dims = bp.mTileSpatialGrid.getDims();
    if (!visited.getBit(nextTileIndex)) {
        RoomNodeID owner = bp.ownerArray[tileIndex];
        RoomNodeID nextOwner = bp.ownerArray[nextTileIndex];
        visited.setBit(nextTileIndex);
        if (nextOwner == owner) {
            bfs[bfsBackIndex++].index = nextTileIndex;
            if (bfsBackIndex >= bfs.size()) bfsBackIndex = 0;
        }
        else if (bp.tiles[tileIndex] == BlueprintTileType::FLOOR) {

            if (nextOwner == INVALID_ROOM_ID) {
                // Exterior doors
                if (canConnectToOutside) {
                    canConnectToOutside = false;
                    TileWall wall;
                    wall.wallID = bp.tileIDs[e_cast(BlueprintTileType::DOOR)];
                    wall.isDoor = true;
                    bp.walls.setWallAtTile(tileIndex, wall, dir);
                    bp.exteriorDoors[tileIndex] = bp.ownerArray[tileIndex];
                    // Visual log
                    if (visLog) {
                        const f32v3 pos(tileIndex % dims.x, tileIndex % (dims.x * dims.y) / dims.x, (tileIndex / (dims.x * dims.y)) * floorHeight);
                        visLog->addFilledQuad(pos, f32v2(1.0f), color4(1.0f, 0.0f, 0.0f, 0.8f));
                    }
                }
            }
            else if (!isConnected.getBit(nextOwner) && room.numAdjacentRooms < MAX_ADJACENT_ROOMS) {
                // Interior doors
                RoomNode& adjacent = bp.rooms[nextOwner];
                if (adjacent.numAdjacentRooms < MAX_ADJACENT_ROOMS) {

                    if (bp.tiles[nextTileIndex] == BlueprintTileType::FLOOR) {

                        TileWall wall;
                        wall.wallID = bp.tileIDs[e_cast(BlueprintTileType::DOOR)];
                        wall.isDoor = true;
                        bp.walls.setWallAtTile(tileIndex, wall, dir);
                      
                        // Clear out any wall on opposite side
                        isConnected.setBit(adjacent.id);
                        room.adjacentRooms[room.numAdjacentRooms++] = RoomGateInfo{ adjacent.id, nextTileIndex };
                        adjacent.adjacentRooms[adjacent.numAdjacentRooms++] = RoomGateInfo{ bp.ownerArray[tileIndex], tileIndex };
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

void BuildingBlueprintGenerator::placeDoors(BuildingBlueprint& bp, VisualLog* visLog) {
    if (visLog) visLog->nextStep("Place doors");
    // TODO: Re-use memory
    BitArray visited(bp.tiles.size());
    BitArray isConnected(bp.rooms.size());

    const i32v3& dims = bp.mTileSpatialGrid.getDims();
    const f32 floorHeight = bp.mTileSpatialGrid.getFloorHeight();

    // Ringbuffer
    // TODO: Re-use memory
    std::vector<DoorBFSNode> bfs(bp.tiles.size());
    size_t bfsFrontIndex;
    size_t bfsBackIndex;
    bool canConnectToOutside = true;
    for (auto&& room : bp.rooms) {
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
        const i32 startIndex = (i32)room.tilePositions[Random::xorshf96() % room.tilePositions.size()];
        assert(bp.ownerArray[startIndex] == room.id);
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
            RoomNodeID roomId = bp.ownerArray[node.index];
            assert(roomId == room.id);
            if (room.numAdjacentRooms == MAX_ADJACENT_ROOMS) {
                break;
            }

            if (visLog) {
                visLog->addWireQuad(f32v3(pos.x, pos.y, room.floorIndex * floorHeight), f32v2(1.0f), color4(0.8f, 0.8f, 0.8f, 0.5f));
            }

            // Bottom
            if (pos.y > 0) {
                doorBfs(bfs, bfsBackIndex, bp, Cartesian::SOUTH, node.index, room, pos, visited, isConnected, canConnectToOutside, visLog);
            }

            // Left
            if (pos.x > 0) {
                doorBfs(bfs, bfsBackIndex, bp, Cartesian::WEST, node.index, room, pos, visited, isConnected, canConnectToOutside, visLog);
            }

            // Right
            if (pos.x < dims.x - 1) {
                doorBfs(bfs, bfsBackIndex, bp, Cartesian::EAST, node.index, room, pos, visited, isConnected, canConnectToOutside, visLog);
            }

            // Up
            if (pos.y < dims.y - 1) {
                doorBfs(bfs, bfsBackIndex, bp, Cartesian::NORTH, node.index, room, pos, visited, isConnected, canConnectToOutside, visLog);
            }
            ++bfsFrontIndex;
        }
    }
}

void BuildingBlueprintGenerator::buildRoomInteriorEdges(BuildingBlueprint& bp, VisualLog* visLog) {
    PreciseTimer timer;
    if (visLog) visLog->nextStep("Build interior edges");
    // TODO: Room minimum AABB?
    BitArray bits;
    const f32 floorHeight = bp.mTileSpatialGrid.getFloorHeight();
    const i32v2& floorDims = bp.mTileSpatialGrid.getDims2D();
    bits.resize(floorDims.x * floorDims.y);
    for (auto&& room : bp.rooms) {
   
        TileIndex tileIndex = room.floorIndex * floorDims.x * floorDims.y;
        for (i32 y = 0; y < floorDims.y; ++y) {
            for (i32 x = 0; x < floorDims.x; ++x) {
                bits.setBitTo(y * floorDims.x + x, bp.ownerArray[tileIndex] == room.id && bp.tiles[tileIndex] == BlueprintTileType::FLOOR);
                ++tileIndex;
            }
        }
        room.interiorEdges = GridEdgeFinder::getInteriorEdgesFromOwnershipArray(bits, floorDims, visLog, room.floorIndex * floorHeight);
        room.edgeWalk = GridEdgeFinder::getInteriorCounterClockwiseWalkFromGridEdges(room.interiorEdges, floorDims);
    }

    if (visLog) {
        visLog->nextStep("Debug interior edges");
        for (auto&& room : bp.rooms) {
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
        for (auto&& room : bp.rooms) {
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

bool tileBlocksDoor(TileIndex index, BuildingBlueprint& bp) {
    const i32v2& floorDims = bp.mTileSpatialGrid.getDims2D();
    const i32v2 pos = getPosAtIndex2D(index, floorDims);
    assert(pos.x > 0 && pos.x < floorDims.x - 1 && pos.y > 0 && pos.y < floorDims.y - 1); // We should have a wall buffer guarenteed
    TileWalls walls;
    bp.walls.getWallsAtTile(walls, index);
    for (int i = 0; i < 4; ++i) {
        if (isDoor(walls.walls[i].wallID)) {
            return true;
        }
    }
    
    return false;
}

bool canPlaceStairsHere(TileIndex index, BuildingBlueprint& bp, RoomNode& child) {
    const i32v2& floorDims = bp.mTileSpatialGrid.getDims2D();
    const TileIndex aboveIndex = index + floorDims.x * floorDims.y;
    if (bp.tiles[index] == BlueprintTileType::FLOOR &&
        bp.ownerArray[aboveIndex] == child.id &&
        bp.tiles[aboveIndex] == BlueprintTileType::FLOOR &&
        !tileBlocksDoor(index, bp) && !tileBlocksDoor(aboveIndex, bp)) {
        return true;
    }
    return false;
}

bool isAtWallCorner(TileIndex index, BuildingBlueprint& bp) {
    const i32v2& floorDims = bp.mTileSpatialGrid.getDims2D();
    const i32v2 pos = getPosAtIndex2D(index, floorDims);
    assert(pos.x > 0 && pos.x < floorDims.x - 1 && pos.y > 0 && pos.y < floorDims.y - 1); // We should have a wall buffer guarenteed
    i32 adjacentWallCount = 0;
    TileWalls walls;
    bp.walls.getWallsAtTile(walls, index);
    for (int i = 0; i < 4; ++i) {
        if (walls.walls[i].isValid()) {
            ++adjacentWallCount;
        }
    }
    // TODO: This disallows single block hallways
    return adjacentWallCount >= 2;
}

bool isRunningIntoWallAtEnd(TileIndex index, BuildingBlueprint& bp, Cartesian dir) {
    const i32v2& floorDims = bp.mTileSpatialGrid.getDims2D();
    i16v2 pos = getPosAtIndex2D(index, floorDims);
    const i32 floorIndex = index / (floorDims.x * floorDims.y);
    assert(pos.x > 0 && pos.x < floorDims.x - 1 && pos.y > 0 && pos.y < floorDims.y - 1); // We should have a wall buffer guarenteed
    pos += CARTESIAN_NORMALS_2D[e_cast(dir)];
    return bp.tiles[getIndex2DAtPos(pos, floorDims, floorIndex + 1)] != BlueprintTileType::FLOOR;
}

void BuildingBlueprintGenerator::placeStairs(BuildingBlueprint& bp, VisualLog* visLog) {
    if (visLog) visLog->nextStep("Place stairs");

    boost::container::static_vector<TileIndex, 256> runs;
    boost::container::static_vector<ui8, 256> runStarts;
    boost::container::static_vector<ui8, 256> runLengths;
    boost::container::static_vector<Cartesian, 256> dirs;
    const i32v2& floorDims = bp.mTileSpatialGrid.getDims2D();
    const i32 floorSize = floorDims.x * floorDims.y;
    const f32 floorHeight = bp.mTileSpatialGrid.getFloorHeight();
    BitArray usedTiles;
    usedTiles.resize(floorSize);
    for (auto&& room : bp.rooms) {
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
            if (bp.rooms[id].floorIndex == room.floorIndex + 1) {
                child = &bp.rooms[id];
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
            if (currentRunLength < room.edgeWalk.size() && canPlaceStairsHere(index, bp, *child) &&
                (currentRunLength == 0 || index != runs.back() /*make sure we dont double back*/) &&
                (currentRunLength != 0 || !isAtWallCorner(index, bp))) {
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
        const i32 STAIRS_UP_COUNT = bp.mTileSpatialGrid.getFloorHeight() + 1;
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
                            if (isRunningIntoWallAtEnd(index, bp, dir)) {
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
                    if (!isRunningIntoWallAtEnd(runs[(i32)(runStart + j)], bp, dir)) {
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
        auto& stairs = bp.stairs.emplace_back();
        stairs.reserve(stairs.size() + bestRunLength);
        i32 height = 0;
        Cartesian prevDir = Cartesian::NONE;
        for (i32 j = 0; j < bestRunLength; ++j) {
            const TileIndex tileIndex = runs[bestRunStart + j];
            bp.tiles[tileIndex] = BlueprintTileType::STAIRS;
            if (j > 0) {
                bp.tiles[tileIndex + floorSize] = BlueprintTileType::AIR;
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

void BuildingBlueprintGenerator::buildExteriorWallRuns(BuildingBlueprint& bp, VisualLog* visLog) {

    if (visLog) visLog->nextStep("Exterior Wall Runs");

    std::vector<ExteriorWallRun>& exteriorWallRuns = bp.exteriorWallRuns;
    exteriorWallRuns.reserve(32);

    const i32v2& dims = bp.mTileSpatialGrid.getDims2D();
    const i32 floorSize = dims.x * dims.y;
    const f32 floorHeight = bp.mTileSpatialGrid.getFloorHeight();
    // Helper function for checking if tile is unowned and therefore external (TODO: Owned could still be external in the garden)
    auto isTileExternal = [&](i32v2 outerPos, i32 floorIndex) -> bool {
        if (outerPos.x < 0 || outerPos.y < 0 || outerPos.x >= dims.x || outerPos.y >= dims.y) {
            return true;
        }
        return bp.ownerArray[outerPos.x + outerPos.y * dims.x + floorIndex * floorSize] == INVALID_ROOM_ID;
    };

    for (auto&& room : bp.rooms) {
        if (room.edgeWalk.size() <= 4) { // Fairly arbitrary, this could be larger
            continue;
        }
        // Edge walk is guaranteed to always begin on a southern edge
        Cartesian currentEdgeNormal = Cartesian::SOUTH;
        TileIndex prevInteriorTileIndex = room.edgeWalk[0];
        i32v2 prevOffset2D = bp.mTileSpatialGrid.getTileXYOffset(prevInteriorTileIndex);
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
            const i32v2 currentOffset2D = bp.mTileSpatialGrid.getTileXYOffset(currentInteriorTileIndex);

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
        const i32v2 finalOffset2D = bp.mTileSpatialGrid.getTileXYOffset(finalTileIndex);
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
            i32v3 startOffset = bp.mTileSpatialGrid.getTileXYZOffset(wallRun.start) + CARTESIAN_TILE_EDGE_WALK_CCW_POSITION_OFFSETS_3D[e_cast(wallRun.dir)];
            startOffset.z *= floorHeight;
            visLog->addLineBetweenPoints(startOffset, startOffset + i32v3(edgeOffset.x, edgeOffset.y, 0), CARTESIAN_COLORS[e_cast(wallRun.dir)]);
        }
    }

    // Compress memory
    exteriorWallRuns.shrink_to_fit();
}

void BuildingBlueprintGenerator::placeWindows(BuildingBlueprint& bp, VisualLog* visLog) {

    if (visLog) visLog->nextStep("Place Windows");
    
    const i32v2& floorDims = bp.mTileSpatialGrid.getDims2D();
    const f32 floorHeight = bp.mTileSpatialGrid.getFloorHeight();
    const i32 INDEX_OFFSETS[4] = {
        1, //South
        -floorDims.x, //West
        floorDims.x, //East
        -1 //North
    };

    for (ExteriorWallRun& wallRun : bp.exteriorWallRuns) {
        if (wallRun.length > 2 && wallRun.length <= MAX_EXTERIOR_WALL_RUN_LENGTH) {
            const ui32 permutation = Random::xorshf96() % sPossibleWindowPermutations[wallRun.length].size();
            const std::vector<bool>& windowPlacements = sPossibleWindowPermutations[wallRun.length][permutation];
            assert(windowPlacements.size() == wallRun.length);
            TileIndex index = wallRun.start;
            for (ui32 i = 0; i < wallRun.length; ++i) {
                if (windowPlacements[i]) {
                    TileWall wall = bp.walls.getWallAtTile(index, wallRun.dir);
                    // Don't ever replace doors
                    if (!wall.isDoor) {
                        bp.walls.setWallAtTile(index, TileWall{ bp.tileIDs[e_cast(BlueprintTileType::WINDOW)], false /*isDoor*/ }, wallRun.dir);
                        if (visLog) {
                            i32v3 offset = bp.mTileSpatialGrid.getTileXYZOffset(index);
                            offset.z *= floorHeight;
                            visLog->addFilledQuad(offset, f32v2(1.0f), color::Aqua);
                        }
                    }
                    else if (visLog) {
                        i32v3 offset = bp.mTileSpatialGrid.getTileXYZOffset(index);
                        offset.z *= floorHeight;
                        visLog->addFilledQuad(offset, f32v2(1.0f), color::Red);
                    }
                }
                if (visLog) {
                    i32v3 offset = bp.mTileSpatialGrid.getTileXYZOffset(index);
                    offset.z *= floorHeight;
                    visLog->addWireQuad(offset, f32v2(1.0f), color::Red);
                }
                index += INDEX_OFFSETS[e_cast(wallRun.dir)];
            }
        }
    }
}

void computeOwnedTilesOnFirstFloor(BuildingBlueprint& bp) {
    const i32v2& floorDims = bp.mTileSpatialGrid.getDims2D();
    bp.tilesNeedingTerrainFlatten = BitArray(floorDims.x * floorDims.y);
    for (ui32 y = 0; y < floorDims.y; ++y) {
        for (ui32 x = 0; x < floorDims.x; ++x) {
            const ui32 tileIndex = y * floorDims.x + x;
            const BlueprintTileType type = bp.tiles[tileIndex];
            if (type != BlueprintTileType::NONE) {

                const TileID tileId = bp.tileIDs[e_cast(type)];
                if (tileId != TILE_ID_NONE) {
                    bp.tilesNeedingTerrainFlatten.setBitTo(tileIndex, true);
                }
            }
        }
    }
}

void BuildingBlueprintGenerator::postProcessBlueprint(BuildingBlueprint& bp) {
    // Tally required items
    std::map<ItemID, ui32> requiredItems;
    TileRepository& tileRepo = TileRepository::get();
    bp.tileRecipes[e_cast(BlueprintTileType::NONE)] = nullptr;
    bp.tileRecipes[e_cast(BlueprintTileType::FLOOR)] = &tileRepo.getRecipeForTile(bp.tileIDs[e_cast(BlueprintTileType::FLOOR)]);
    bp.tileRecipes[e_cast(BlueprintTileType::DOOR)] = &tileRepo.getRecipeForTile(bp.tileIDs[e_cast(BlueprintTileType::DOOR)]);
    bp.tileRecipes[e_cast(BlueprintTileType::WALL)] = &tileRepo.getRecipeForTile(bp.tileIDs[e_cast(BlueprintTileType::WALL)]);
    bp.tileRecipes[e_cast(BlueprintTileType::WINDOW)] = &tileRepo.getRecipeForTile(bp.tileIDs[e_cast(BlueprintTileType::WINDOW)]);
    bp.tileRecipes[e_cast(BlueprintTileType::STAIRS)] = &tileRepo.getRecipeForTile(bp.tileIDs[e_cast(BlueprintTileType::STAIRS)]);
    bp.tileRecipes[e_cast(BlueprintTileType::STAIRS_FLAT)] = &tileRepo.getRecipeForTile(bp.tileIDs[e_cast(BlueprintTileType::STAIRS_FLAT)]);
    bp.tileRecipes[e_cast(BlueprintTileType::AIR)] = nullptr;
    static_assert(e_cast(BlueprintTileType::TYPES) == 8);

    // Guess
    bp.tileItemData.reserve(bp.tiles.size() / 2);
    for (TileIndex tileIndex = 0; tileIndex < (TileIndex)bp.tiles.size(); ++tileIndex) {
       
        // Tile postprocess
        switch (bp.tiles[tileIndex]) {
            case BlueprintTileType::NONE:
            case BlueprintTileType::AIR:
                bp.tileBuildData[tileIndex].mProgress = 1.0f;
                break;
            case BlueprintTileType::STAIRS:
            case BlueprintTileType::STAIRS_FLAT:
            case BlueprintTileType::WALL:
            case BlueprintTileType::WINDOW:
            case BlueprintTileType::FLOOR:
            case BlueprintTileType::DOOR: {
                const Recipe& recipe = *bp.tileRecipes[e_cast(bp.tiles[tileIndex])];
                const ui32 offset = bp.tileItemData.size();
                const ui16 itemCount = recipe.mItemCount;
                bp.tileItemData.reserve(bp.tileItemData.size() + itemCount);
                bp.tileItemDataHandles[tileIndex] = BlueprintTileItemDataHandle{ offset, itemCount };
                for (ui32 r = 0; r < recipe.mItemCount; ++r) {
                    const ItemStack stack = recipe.mItems[r];
                    auto&& it = requiredItems.find(stack.id);
                    if (it == requiredItems.end()) {
                        requiredItems[stack.id] = stack.quantity;
                    }
                    else {
                        it->second += stack.quantity;
                    }
                    bp.tileItemData.emplace_back(BlueprintTileItemData{stack.id, stack.quantity, 0});
                    // We will pull from back so lets emplace front to make it a FIFO
                    bp.tilesNeedingItems[stack.id].emplace_front(tileIndex);
                }
                ++bp.totalTilesToBuild;
                break;
            }
            case BlueprintTileType::TYPES:
            default:
                assert(false);
                break;
        }
    }
    static_assert(e_cast(BlueprintTileType::TYPES) == 8);

    for (auto&& it : requiredItems) {
        bp.requiredItemsToBuild.push_back(ItemStackUnbounded{ it.first, (ui32)it.second });
    }

    computeOwnedTilesOnFirstFloor(bp);
    // TODO: Sort bp.tilesToBuild by distance from entrances
}

BuildingBlueprintId BuildingBlueprintGenerator::getNextBuildingID() {
    static std::mutex sMutex;
    std::lock_guard lock(sMutex);
    ++sCurrentId;
    // Will this ever happen? maybe...
    if (sCurrentId == INVALID_BLUEPRINT_ID) {
        sCurrentId = 0;
    }
    return sCurrentId;
}
