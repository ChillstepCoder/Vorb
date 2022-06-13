#include "stdafx.h"
#include "BuildingBlueprintGenerator.h"
#include "BuildingDescriptionRepository.h"

#include "city/CityBuilder.h"

#include "resources/TileRepository.h"

#include <Vorb/Timing.h>
#include "Random.h"

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
    visLog.addWireQuad(f32v3(0.0f, 0.0f, 0.0f), bp.aabb.dims, inputColor ? *inputColor : color4(0.7f, 0.4f, 0.0f));

    // AABBS first
    int i = 0;
    for (auto&& node : bp.rooms) {

        const color4& color = ROOM_COLORS[i % MAX_ROOM_COLORS];
        visLog.addWireQuad(f32v3(node.aabb.x - bp.aabb.x, node.aabb.y - bp.aabb.y, node.floorIndex * bp.floorHeight), node.aabb.dims, color4(color.r, color.g, color.b, 255u));
        // Draw parent line
        if (node.parentRoom != INVALID_ROOM_ID) {
            RoomNode& parent = bp.rooms[node.parentRoom];
            const f32v3 startPos(node.offsetFromZero.x + 0.5f, node.offsetFromZero.y + 0.5f, node.floorIndex * bp.floorHeight);
            const f32v3 endPos(parent.offsetFromZero.x + 0.5f, parent.offsetFromZero.y + 0.5f, parent.floorIndex * bp.floorHeight);
            if (node.isPrivate) {
                visLog.addLineBetweenPoints(startPos, endPos, color4(1.0f, 1.0f, 0.0f));
            }
            else {
                visLog.addLineBetweenPoints(startPos, endPos, color4(0.0f, 1.0f, 0.0f));
            }
        }
        ++i;
    }

    // Render all the tiles
    for (int z = 0; z < bp.floorCount; ++z) {
        const ui32 floorIndex = z * bp.aabb.dims.x * bp.aabb.dims.y;
        for (int y = 0; y < bp.aabb.dims.y; ++y) {
            for (int x = 0; x < bp.aabb.dims.x; ++x) {
                const TileIndex index = floorIndex + y * bp.aabb.dims.x + x;
                RoomNodeID id = bp.ownerArray[index];
                if (id != INVALID_ROOM_ID) {
                    const RoomNode& room = bp.rooms[id];
                    const f32v2 pos = f32v2(x, y);
                    const color4& color = inputColor ? *inputColor : ROOM_COLORS[id % MAX_ROOM_COLORS];
                    visLog.addFilledQuad(f32v3(pos.x, pos.y, room.floorIndex * bp.floorHeight), f32v2(1.0f), color4(color.r, color.g, color.b, 128u));
                }
            }
        }
    }

}

inline bool boundsCheckTile(const i16v2& pos, const ui32AABB2& aabb) {
    return pos.x >= 0 && pos.x < aabb.width&& pos.y >= 0 && pos.y < aabb.depth;
}

// Dont place on outer border, thats where facade goes
bool boundsCheckRoom(i16 pos, i16 dim) {
    return pos >= 1 && pos < dim - 1;
}


BuildingBlueprintGenerator::BuildingBlueprintGenerator(BuildingDescriptionRepository& buildingRepo, CityBuilder& cityBuilder) :
    mBuildingRepo(buildingRepo),
    mCityBuilder(cityBuilder)
{

}

std::unique_ptr<BuildingBlueprint> BuildingBlueprintGenerator::generateBlueprintAsyncThenSendToBuilder(const BuildingDef& desc, float sizeAlpha, Cartesian entrySide, ui16v2 plotSize, const ui32v2& bottomLeftPos, entt::entity ownerEntity, BuildingBlueprintFlags flags, f32 zPosApprox)
{
    assert(desc.publicRoomCountRange.y != 0.0f);
    BuildingBlueprintId id = getNextBuildingID();


    std::unique_ptr<BuildingBlueprint> bp = std::make_unique<BuildingBlueprint>(desc, sizeAlpha, entrySide, plotSize, bottomLeftPos, ownerEntity, flags);
    assert(plotSize.x > 2 && plotSize.y > 2);
    bp->id = id;
    bp->zPos = zPosApprox;
    BuildingBlueprint* bPtr = bp.get();
    mGeneratingBuildings.insert(bPtr);
    
    Services::Threadpool::ref().addTask([&, bPtr](ThreadPoolWorkerData* workerData) {
        generateBlueprintInternal(bPtr, mBuildingRepo);

    }, [&, bPtr]() {
        // Main thread
        mGeneratingBuildings.erase(bPtr);
        bPtr->isGenerating = false;
        mCityBuilder.addBlueprintToBuildAndPreprocess(bPtr);
    });
    return bp;
}

std::unique_ptr<BuildingBlueprint> BuildingBlueprintGenerator::generateBlueprintSync(BuildingDescriptionRepository& buildingRepo, const BuildingDef& desc, float sizeAlpha, Cartesian entrySide, ui16v2 plotSize, const ui32v2& bottomLeftPos, entt::entity ownerEntity, BuildingBlueprintFlags flags, f32 zPosApprox) {
    BuildingBlueprintId id = getNextBuildingID();
    std::unique_ptr<BuildingBlueprint> bp = std::make_unique<BuildingBlueprint>(desc, sizeAlpha, entrySide, plotSize, bottomLeftPos, ownerEntity, flags);
    assert(plotSize.x > 2 && plotSize.y > 2);
    bp->id = id;
    bp->zPos = zPosApprox;
    generateBlueprintInternal(bp.get(), buildingRepo);
    return bp;
}

void BuildingBlueprintGenerator::generateBlueprintInternal(BuildingBlueprint* bPtr, BuildingDescriptionRepository& buildingRepo) {

    VisualLog* visLog = VisualLogger::tryGetNewVisualLog("Blueprint");
    if (visLog) {
        visLog->setRootPos(f32v3(bPtr->aabb.pos.x, bPtr->aabb.pos.y, bPtr->zPos));
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
    expandRooms(*bPtr, visLog);
    roomCleanup(*bPtr, visLog);

    // Walls
    placeFacadeWalls(*bPtr, visLog);
    placeInteriorWalls(*bPtr, visLog);

    // Room edge info
    buildRoomInteriorEdges(*bPtr, visLog);

    // Doors
    // placeHallwayDoors
    placeDoors(*bPtr, visLog);

    // Stairs
    if (!placeStairs(*bPtr, visLog)) {
        pError("Failed to place stairs!");
    }

    // Furniture

    // Flooring

    // Tally final item requirements
    postProcessBlueprint(*bPtr);

    if (visLog) {
        // Draw the entire room graph
        renderBlueprintDebugVislog(*bPtr, *visLog, nullptr);
        visLog->finish();
    }
}

void BuildingBlueprintGenerator::addPublicRoomsToGraph(BuildingBlueprint& bp) {
    // Generate public room structure using grammar
    ui32 publicRoomCount = bp.desc.publicRoomCountRange.y <= bp.desc.publicRoomCountRange.x ?
        bp.desc.publicRoomCountRange.x : Random::xorshf96() % (bp.desc.publicRoomCountRange.y - bp.desc.publicRoomCountRange.x) + bp.desc.publicRoomCountRange.x;
    assert(publicRoomCount); // Must have at least one public room
    bp.rooms.resize(publicRoomCount);
    bp.desc.publicGrammar.buildRoomGraph(bp.rooms);
}

void BuildingBlueprintGenerator::assignPublicRooms(BuildingBlueprint& bp)
{
    assert(bp.desc.publicRooms.size());
    ui8v2 countLookup[255]; // (current, max)
    ui32 publicRoomCount = (ui32)bp.desc.publicRooms.size();
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
            node.roomDefId = bp.desc.publicRooms[roomIndex++].id;
            
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
    ui32 privateRoomCount = round(bp.sizeAlpha * (bp.desc.privateRoomCountRange.y - bp.desc.privateRoomCountRange.x) + bp.desc.privateRoomCountRange.x);
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

    bp.rooms.reserve(bp.rooms.size() + privateRoomCount);
    int failCount = 0;
    int publicIndex = Random::xorshf96() % numPublicRooms;
    int privateIndex = 0;
    for (size_t i = 0; i < privateRoomCount; ++i) {
        RoomNode& publicRoom = bp.rooms[publicIndex];
        if (publicRoom.numChildren < MAX_CHILD_ROOMS && countLookup[privateIndex].x < countLookup[privateIndex].y) {
            // We can fit a private room here
            // Next node index is our child
            publicRoom.childRooms[publicRoom.numChildren++] = (RoomNodeID)bp.rooms.size();
            // Append the room
            RoomNode privateRoom;
            privateRoom.roomDefId = bp.desc.privateRooms[privateIndex].id;
            privateRoom.parentRoom = publicIndex;
            privateRoom.isPrivate = true;
            bp.rooms.emplace_back(std::move(privateRoom));
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
        const ui16 depth = getMaximumDepthRecursive(nodes, &nodes[node->childRooms[i]]);
        if (depth > maximumChildDepth) {
            maximumChildDepth = depth;
        }
    }
    return maximumChildDepth + 1;
}

void placeChildrenRecursive(BuildingBlueprint& bp, RoomNode* node, f32 availableWidthSpan, ui32 maxXOffsetPerLayer, ui32v2 currentOffset, const ui32v2& dims2d, VisualLog* visLog) {
    if (node->numChildren == 0) {
        return;
    }
    assert(currentOffset.x < 10000 && currentOffset.y < 10000);
    std::vector<RoomNode>& nodes = bp.rooms;

    // TODO: Worry about even vs odd?
    const ui16 myDesiredRadius = node->desiredWidth / 2;

    // Determine desired child y span
    ui16 totalChildSpan = 0;
    for (int i = 0; i < node->numChildren; ++i) {
        RoomNode& child = nodes[node->childRooms[i]];
        totalChildSpan += child.desiredWidth;
    }

    const f32 desiredWidthSpan = vmath::min((f32)totalChildSpan, availableWidthSpan);
    
    // Place a child above with stairs if possible
    bool didCreateStairs = false;
    for (int i = 0; i < node->numChildren; ++i) {
        RoomNode& child = nodes[node->childRooms[i]];
        child.floorIndex = node->floorIndex;
        // New floor TODO: Random? Statistics?
        if (node->roomDef->stairsChance && child.roomDef->canStairsConnect && child.desiredWidth >= 3) {
            if (Random::getCachedRandomf() > node->roomDef->stairsChance) {
                didCreateStairs = true;
                node->hasStairs = true;

                child.offsetFromZero = currentOffset;
                child.floorIndex = node->floorIndex + 1;
                if (child.floorIndex == bp.floorCount) {
                    ++bp.floorCount;
                }
                // Visual log
                if (visLog) {
                    const color4& color = ROOM_COLORS[node->childRooms[i] % MAX_ROOM_COLORS];
                    const f32v3 childPos(child.offsetFromZero.x, child.offsetFromZero.y, child.floorIndex * bp.floorHeight);
                    visLog->addWireQuad(childPos, f32v2(1.0f), color);
                    const f32v3 parentPos(node->offsetFromZero.x, node->offsetFromZero.y, node->floorIndex * bp.floorHeight);
                    visLog->addLineBetweenPoints(childPos, parentPos, color);
                }
                placeChildrenRecursive(bp, &child, dims2d.y, maxXOffsetPerLayer, child.offsetFromZero, dims2d, visLog);
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
        currentOffset.y -= (ui16)(widthSegmentSize * (node->numChildren - 1));
        for (int i = 0; i < node->numChildren; ++i) {
            RoomNode& child = nodes[node->childRooms[i]];
            // If above, continue since we did this already
            if (child.floorIndex == node->floorIndex + 1) {
                continue;
            }

            const ui16 childDesiredRadius = child.desiredWidth / 2;

            ui16 xOffset = vmath::min(maxXOffsetPerLayer, (ui32)(myDesiredRadius + childDesiredRadius));
            if (xOffset < 1) xOffset = 1;

            child.offsetFromZero = currentOffset;
            child.offsetFromZero.x += xOffset;
            // Visual log
            if (visLog) {
                const color4& color = ROOM_COLORS[node->childRooms[i] % MAX_ROOM_COLORS];
                const f32v3 childPos(child.offsetFromZero.x, child.offsetFromZero.y, child.floorIndex * bp.floorHeight);
                visLog->addWireQuad(childPos, f32v2(1.0f), color);
                const f32v3 parentPos(node->offsetFromZero.x, node->offsetFromZero.y, node->floorIndex * bp.floorHeight);
                visLog->addLineBetweenPoints(childPos, parentPos, color);
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
        room.desiredWidth = (ui16)round(lerp((f32)desc.minWidth, (f32)desc.maxWidth, bp.sizeAlpha));
        room.desiredSize = room.desiredWidth * room.desiredWidth; //SQ
    }
}

void applyForceOffset(ui16v2& offset, const f32v2& force, const ui16v2& dims) {
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
    ui32 maximumDepth = getMaximumDepthRecursive(bp.rooms, root);

    // Determine which dims to use for cartesian
    ui32v2 dims;
    switch (bp.entrySide) {
        case Cartesian::SOUTH:
        case Cartesian::NORTH:
            dims.x = bp.aabb.dims.y;
            dims.y = bp.aabb.dims.x;
            break;
        case Cartesian::WEST:
        case Cartesian::EAST:
            dims = bp.aabb.dims;
            break;
    }
    const ui32 maxDepthOffsetPerLayer = dims.x / maximumDepth;
    f32 availableWidthSpan = dims.y;
    // Place the root, +1 so we are less likely to touch the side of the AABB
    root->offsetFromZero = i32v2(vmath::min(maxDepthOffsetPerLayer / 2, (ui32)root->desiredWidth / 2) + 1, dims.y / 2);
    if (root->offsetFromZero.x == 0) root->offsetFromZero.x = 1u;
    assert(root->offsetFromZero.x < 10000 && root->offsetFromZero.y < 10000);

    // We will generate to the right, then will rotate the coordinates around based on the cartesian
    placeChildrenRecursive(bp, root, availableWidthSpan, maxDepthOffsetPerLayer, root->offsetFromZero, dims, visLog);

    // Rotate all coordinates around for Cartesian direction
    // Left is the base case so do nothing for that
    switch (bp.entrySide) {
        case Cartesian::SOUTH:
            for (auto&& room : bp.rooms) {
                ui16 tmp = room.offsetFromZero.x;
                room.offsetFromZero.x = room.offsetFromZero.y;
                room.offsetFromZero.y = bp.aabb.dims.y - tmp - 1;
            }
            break;
        case Cartesian::EAST:
            for (auto&& room : bp.rooms) {
                room.offsetFromZero.x = bp.aabb.dims.x - room.offsetFromZero.x - 1;
            }
            break;
        case Cartesian::NORTH:
            for (auto&& room : bp.rooms) {
                std::swap(room.offsetFromZero.x, room.offsetFromZero.y);
                room.offsetFromZero.x = bp.aabb.dims.x - room.offsetFromZero.x - 1;
            }
            break;
    }

    // Clamp positions to be withing facade
    for (auto&& room : bp.rooms) {
        room.offsetFromZero.x = vmath::clamp((ui32)room.offsetFromZero.x, (ui32)1u, bp.aabb.dims.x);
        room.offsetFromZero.y = vmath::clamp((ui32)room.offsetFromZero.y, (ui32)1u, bp.aabb.dims.y);
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
                    applyForceOffset(room1.offsetFromZero, -pushForce, bp.aabb.dims);
                    applyForceOffset(room2.offsetFromZero, pushForce, bp.aabb.dims);
                } else if (room2.parentRoom == i) { // Magnet only to children
                    const f32v2 pullForce = offset * ((distance - desiredDistance) * FORCE_MULT);
                    applyForceOffset(room1.offsetFromZero, pullForce, bp.aabb.dims);
                    applyForceOffset(room2.offsetFromZero, -pullForce, bp.aabb.dims);
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
                    applyForceOffset(room1.offsetFromZero, -pushForce, bp.aabb.dims);
                    applyForceOffset(room2.offsetFromZero, pushForce, bp.aabb.dims);
                }
            }
        }
    }
}

// Helper
inline ui32 getIndexAtPos(ui32v2 pos, ui32v2 dims, ui16 floorIndex) {
    return (ui32)floorIndex * dims.y * dims.x + pos.y * dims.x + pos.x;
}

inline ui32 getIndexAtPos(i16v2 pos, ui32v2 dims, ui16 floorIndex) {
    return (ui32)floorIndex * dims.y * dims.x + pos.y * dims.x + pos.x;
}

inline ui32 getIndexAtPos(ui32 x, ui32 y, ui32v2 dims, ui16 floorIndex) {
    return (ui32)floorIndex * dims.y * dims.x + y * dims.x + x;
}

inline ui32v2 getPosAtIndex(ui32 tileIndex, ui32v2 dims) {
    ui32 floorStride = dims.x * dims.y;
    return ui32v2(tileIndex % dims.x, (tileIndex % floorStride) / dims.x);
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

RoomBorderOuterDir OPPOSITE_WALL_DIRS[4] = {
    RoomBorderOuterDir::RIGHT,  // LEFT
    RoomBorderOuterDir::BOTTOM, // TOP
    RoomBorderOuterDir::LEFT,   // RIGHT
    RoomBorderOuterDir::TOP     // BOTTOM
};

void extendWallStart(RoomBorder& wall, const i16v2& offset) {
    wall.startPos += offset;
    assert(wall.startPos.x >= 0 && wall.startPos.y >= 0);
    ++wall.length;
}

void extendWallEnd(RoomBorder& wall, const i16v2& offset) {
    wall.endPos += offset;
    ++wall.length;
}


void expandWall(RoomBorder& wall, BuildingBlueprint& bp, RoomNode& room) {
    const i16v2& expandOffset = EXPAND_OFFSETS[e_cast(wall.outerDir)];
    const i16v2& iterateOffset = ITERATE_OFFSETS[e_cast(wall.outerDir)];

    wall.startPos += expandOffset;
    assert(wall.startPos.x >= 0 && wall.startPos.y >= 0);
    wall.endPos += expandOffset;
    extendWallEnd(*wall.startAdjacent, expandOffset);
    extendWallStart(*wall.endAdjacent, expandOffset);

    // Set new metadata
    i16v2 outerPos = wall.startPos;
    for (int j = 0; j < wall.length; ++j) {
        const ui32 index = getIndexAtPos(outerPos, bp.aabb.dims, room.floorIndex);
        RoomNodeID ownerId = bp.ownerArray[index];
        if (ownerId != INVALID_ROOM_ID) {
            RoomNode& ownerRoom = bp.rooms[ownerId];
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
void expandWallGapsOnly(RoomBorder& wall, BuildingBlueprint& bp, RoomNode& room, VisualLog* visLog) {
    const i16v2& expandOffset = EXPAND_OFFSETS[e_cast(wall.outerDir)];
    const i16v2& iterateOffset = ITERATE_OFFSETS[e_cast(wall.outerDir)];

    wall.startPos += expandOffset;
    assert(wall.startPos.x >= 0 && wall.startPos.y >= 0);
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
        const ui32 index = getIndexAtPos(outerPos, bp.aabb.dims, room.floorIndex);
        RoomNodeID ownerId = bp.ownerArray[index];
        if (ownerId == INVALID_ROOM_ID) {
            ++sizeAdd;
            bp.ownerArray[index] = room.id;
            bp.tiles[index].type = BlueprintTileType::FLOOR;
            // Visual log
            if (visLog) {
                visLog->addWireQuad(f32v3(outerPos.x, outerPos.y, room.floorIndex * bp.floorHeight), f32v2(1.0f), ROOM_COLORS[room.id % MAX_ROOM_COLORS]);
            }
        }
        // Step
        outerPos += iterateOffset;
    }

    // TODO: Only along actual expansion tiles
    // TODO: Decrement size when we push into another room
    room.size += wall.length;
}

bool expandRoomSquare(BuildingBlueprint& bp, RoomNode& room, VisualLog* visLog) {

   bool didExpand = false;

    for (int i = 0; i < room.numWalls; ++i) {

        f32 currentPressure = getPressureValue(room);
        // Once we are at desired size, no need to expand
        if (currentPressure <= 1.0f) {
            return didExpand;
        }

        RoomBorder& wall = room.borders[i];
        // Expand
        const i16v2& expandOffset = EXPAND_OFFSETS[e_cast(wall.outerDir)];
        const i16v2& iterateOffset = ITERATE_OFFSETS[e_cast(wall.outerDir)];
        const int xOrY = (int)wall.outerDir % 2;
        const i16v2 nextStart = wall.startPos + expandOffset;
        // Bounds check
        if (boundsCheckRoom(nextStart[xOrY], bp.aabb.dims[xOrY])) {
            assert(wall.length <= MAX_WALL_LENGTH);
            // We will only expand if we arent expanding into another room
            bool canExpand = true;
            i16v2 outerPos = nextStart;
            for (int j = 0; j < wall.length; ++j) {
                const ui32 index = getIndexAtPos(outerPos, bp.aabb.dims, room.floorIndex);
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
                if (visLog) {
                    const f32v3 lineStart(wall.startPos.x + 0.5f, wall.startPos.y + 0.5f, room.floorIndex * bp.floorHeight);
                    const f32v3 lineEnd(wall.endPos.x + 0.5f, wall.endPos.y + 0.5f, room.floorIndex * bp.floorHeight);
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

    for (int iter = 0; iter < ITER_STEP; ++iter) {

        // Try expand walls
        int expandCount = 0;
        for (int i = 0; i < room.numWalls; ++i) {

            f32 currentPressure = getPressureValue(room);
            // Once we are at desired size, no need to expand
            if (currentPressure <= 1.0f) {
                return didExpand;
            }

            RoomBorder& wall = room.borders[i];
            // Expand
            const i16v2& expandOffset = EXPAND_OFFSETS[e_cast(wall.outerDir)];
            const i16v2& iterateOffset = ITERATE_OFFSETS[e_cast(wall.outerDir)];
            const int xOrY = (int)wall.outerDir % 2;
            const i16v2 nextStart = wall.startPos + expandOffset;
            // Bounds check
            if (boundsCheckRoom(nextStart[xOrY], bp.aabb.dims[xOrY])) {
                assert(wall.length <= MAX_WALL_LENGTH);
                // We will only expand if there is free space
                bool canExpand = false;
                i16v2 outerPos = nextStart;
                for (int j = 0; j < wall.length; ++j) {
                    // We will expand if there is at least one empty square here
                    const ui32 index = getIndexAtPos(outerPos, bp.aabb.dims, room.floorIndex);
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
                    expandWallGapsOnly(wall, bp, room, visLog);
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

void BuildingBlueprintGenerator::placeFacadeWalls(BuildingBlueprint& bp, VisualLog* visLog) {
    if (visLog) visLog->nextStep("Place facade walls");
    for (RoomNodeID roomId = 0; roomId < bp.rooms.size(); ++roomId) {
        RoomNode& room = bp.rooms[roomId];
        // Iteratively expand walls
        for (int i = 0; i < room.numWalls; ++i) {
            RoomBorder& wall = room.borders[i];
            // Opposite for iteration axis
            const int xOrY = ((int)wall.outerDir + 1) % 2;
            // Deliberately is 1 less than the length
            int wallLength = abs(wall.endPos[xOrY] - wall.startPos[xOrY]);
            const i16v2& iterDir = ITERATE_OFFSETS[e_cast(wall.outerDir)];
            const i16v2& outerDir = EXPAND_OFFSETS[e_cast(wall.outerDir)];
            i16v2 pos = wall.startPos;
            assert(pos.x >= 0 && pos.y >= 0);
            bool finalWasSuccess = false;
            // Iterate along the wall and mark as wall nodes
            for (int j = 0; j <= wallLength; ++j) {
                const ui32 index = getIndexAtPos(pos, bp.aabb.dims, room.floorIndex);
                // Only place wall if we own this tile
                if (bp.ownerArray[index] == roomId) {
                    i16v2 facadePos = pos + outerDir;
                    ui32 facadeIndex = getIndexAtPos(facadePos, bp.aabb.dims, room.floorIndex);
                    // Only place facade if this is an unowned tile
                    if (bp.ownerArray[facadeIndex] == INVALID_ROOM_ID) {
                        bp.tiles[facadeIndex].type = BlueprintTileType::WALL;
                        finalWasSuccess = true;
                        // Visual log
                        if (visLog) {
                            visLog->addWireQuad(f32v3(facadePos.x, facadePos.y, room.floorIndex * bp.floorHeight), f32v2(1.0f), color4(0.0f, 1.0f, 0.0f));
                        }
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
                ui32 facadeIndex = getIndexAtPos(facadePos, bp.aabb.dims, room.floorIndex);
                // Only place facade if this is an unowned tile
                if (bp.ownerArray[facadeIndex] == INVALID_ROOM_ID) {
                    bp.tiles[facadeIndex].type = BlueprintTileType::WALL;
                    // Visual log
                    if (visLog) {
                        visLog->addWireQuad(f32v3(facadePos.x, facadePos.y, room.floorIndex * bp.floorHeight), f32v2(1.0f), color4(0.0f, 1.0f, 0.0f));
                    }
                }
            }
        }
    }
}

void BuildingBlueprintGenerator::placeInteriorWalls(BuildingBlueprint& bp, VisualLog* visLog) {
    if (visLog) visLog->nextStep("Place interior walls");
    // First place main segments
    for (ui32 z = 0; z < bp.floorCount; ++z) {
        for (ui32 y = 1; y < bp.aabb.dims.y - 1; ++y) {
            for (ui32 x = 1; x < bp.aabb.dims.x - 1; ++x) {
                const ui32 index = getIndexAtPos(x, y, bp.aabb.dims, z);
                RoomNodeID roomId = bp.ownerArray[index];
                if (roomId == INVALID_ROOM_ID) {
                    continue;
                }
                int placedWalls = 0;
                { // Right
                    ui32 nIndex = index + 1;
                    RoomNodeID nId = bp.ownerArray[nIndex];
                    if (nId != INVALID_ROOM_ID && nId != roomId) {
                        bp.tiles[nIndex].type = BlueprintTileType::WALL;
                        ++placedWalls;
                        // Visual log
                        if (visLog) {
                            visLog->addWireQuad(f32v3(x + 1, y, z * bp.floorHeight), f32v2(1.0f), color4(0.2f, 0.2f, 1.0f));
                        }
                    }
                }
                { // Down
                    ui32 nIndex = index - bp.aabb.dims.x;
                    RoomNodeID nId = bp.ownerArray[nIndex];
                    if (nId != INVALID_ROOM_ID && nId != roomId) {
                        bp.tiles[nIndex].type = BlueprintTileType::WALL;
                        ++placedWalls;
                        // Visual log
                        if (visLog) {
                            visLog->addWireQuad(f32v3(x, y - 1, z * bp.floorHeight), f32v2(1.0f), color4(0.2f, 0.2f, 1.0f));
                        }
                    }
                }
            }
        }
        // Next place corners
        for (ui32 y = 1; y < bp.aabb.dims.y - 1; ++y) {
            for (ui32 x = 1; x < bp.aabb.dims.x - 1; ++x) {
                ui32 index = getIndexAtPos(x, y, bp.aabb.dims, z);
                if (bp.tiles[index].type != BlueprintTileType::WALL) {
                    continue;
                }
                // Down-right configuration
                {
                    const ui32 downRightIndex = index - bp.aabb.dims.x + 1;
                    if (bp.tiles[downRightIndex].type == BlueprintTileType::WALL) {
                        const ui32 downIndex = index - bp.aabb.dims.x;
                        if (bp.tiles[downIndex].type != BlueprintTileType::WALL) {
                            // Always place to right
                            bp.tiles[index + 1].type = BlueprintTileType::WALL;

                            // Visual log
                            if (visLog) {
                                visLog->addWireQuad(f32v3(x + 1, y - 1, z * bp.floorHeight), f32v2(1.0f), color4(0.2f, 0.2f, 1.0f));
                            }
                        }
                    }
                }
                // Up-right configuration
                {
                    const ui32 upRightIndex = index + bp.aabb.dims.x + 1;
                    if (bp.tiles[upRightIndex].type == BlueprintTileType::WALL) {
                        const ui32 upIndex = index + bp.aabb.dims.x;
                        if (bp.tiles[upIndex].type != BlueprintTileType::WALL) {
                            // Always place to right
                            bp.tiles[index + 1].type = BlueprintTileType::WALL;

                            // Visual log
                            if (visLog) {
                                visLog->addWireQuad(f32v3(x + 1, y + 1, z * bp.floorHeight), f32v2(1.0f), color4(0.2f, 0.2f, 1.0f));
                            }
                        }
                    }
                }
            }
        }
    }
}

void BuildingBlueprintGenerator::expandRooms(BuildingBlueprint& bp, VisualLog* visLog) {
    if (visLog) visLog->nextStep("Expand rooms");
    bp.tiles.resize((size_t)bp.floorCount * bp.aabb.dims.x * bp.aabb.dims.y, BlueprintTile{ BlueprintTileType::NONE, false });
    
    bp.ownerArray.resize(bp.tiles.size(), INVALID_ROOM_ID);

    // Init rooms
    for (size_t i = 0; i < bp.rooms.size(); ++i) {
        initRoomWalls(bp, bp.rooms[i]);
    }

    // Expand walls in square shape, no overwrite
    for (int iters = 0; iters < MAX_WALL_LENGTH; ++iters) {
        int failCount = 0;
        for (size_t i = 0; i < bp.rooms.size(); ++i) {
            failCount += expandRoomSquare(bp, bp.rooms[i], visLog) ? 0 : 1;
        }
        if (failCount == bp.rooms.size()) {
            break;
        }
    }
    // Fill in gaps, no overwrite
    for (int iters = 0; iters < MAX_WALL_LENGTH / ITER_STEP; ++iters) {
        int failCount = 0;
        for (size_t i = 0; i < bp.rooms.size(); ++i) {
            failCount += expandRoomGaps(bp, bp.rooms[i], visLog) ? 0 : 1;
        }
        if (failCount == bp.rooms.size()) {
            break;
        }
    }
}

// Cellular Automata Sub-steps

ui8 ROOM_NODE_COUNT_CACHE[0xff + 0x1] = {};

// NOTE: Iteration order is important! We iterate to the right and then upwards
void FixupSingleRoomPieces(BuildingBlueprint& bp, ui16 x, ui16 y, ui16 index, VisualLog* visLog) {

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
        const RoomNodeID downID = bp.ownerArray[index - bp.aabb.dims.x];
        ++ROOM_NODE_COUNT_CACHE[downID];
        // Left
        const RoomNodeID leftID = bp.ownerArray[index - 1];
        ++ROOM_NODE_COUNT_CACHE[leftID];
        // Right
        const RoomNodeID rightID = bp.ownerArray[index + 1];
        ++ROOM_NODE_COUNT_CACHE[rightID];
        // Top
        const RoomNodeID topID = bp.ownerArray[index + bp.aabb.dims.x];
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
                        visLog->addFilledQuad(f32v3(x, y, bp.rooms[bestId].floorIndex * bp.floorHeight), f32v2(1.0f), ROOM_COLORS[bestId % MAX_ROOM_COLORS]);
                    }
                }

                bp.ownerArray[index] = bestId;
                --bp.rooms[myID].size;
            }
            else if (myID != INVALID_ROOM_ID) {
                // Visual log
                if (visLog) {
                    visLog->addFilledQuad(f32v3(x, y, bp.rooms[myID].floorIndex * bp.floorHeight), f32v2(1.0f), color4(1.0f, 0.0f, 0.0f));
                }
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
                    index -= bp.aabb.dims.x;
                    break;
                case Cartesian::WEST: // left
                    if (x == 1) {
                        return;
                    }
                    --x;
                    --index;
                    break;
                case Cartesian::EAST: // right
                    if (x == bp.aabb.dims.x - 2) {
                        return;
                    }
                    ++x;
                    ++index;
                    break;
                case Cartesian::NORTH: // up
                    if (y == bp.aabb.dims.y - 2) {
                        return;
                    }
                    ++y;
                    index += bp.aabb.dims.x;
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

void BuildingBlueprintGenerator::roomCleanup(BuildingBlueprint& bp, VisualLog* visLog)
{
    constexpr int CELLULAR_AUTOMATA_ITERATIONS = 1;

    if (visLog) visLog->nextStep("Room cleanup");

    for (int step = 0; step < CELLULAR_AUTOMATA_ITERATIONS; ++step) {
        for (ui16 z = 0; z < bp.floorCount; ++z) {
            for (ui16 y = 1; y < bp.aabb.dims.y - 1; ++y) {
                for (ui16 x = 1; x < bp.aabb.dims.x - 1; ++x) {
                    const ui16 index = getIndexAtPos(x, y, bp.aabb.dims, z);
                    FixupSingleRoomPieces(bp, x, y, index, visLog);
                }
            }
        }
    }
}

void BuildingBlueprintGenerator::initRoomWalls(BuildingBlueprint& bp, RoomNode& room)
{
    const ui32 index = getIndexAtPos(ui32v2(room.offsetFromZero), bp.aabb.dims, room.floorIndex);
    // Init root node
    room.size = 1;
    bp.tiles[index].type = BlueprintTileType::FLOOR;
    bp.ownerArray[index] = room.id;

    // Init 4 base walls
    room.numWalls = 4;
    for (int i = 0; i < room.numWalls; ++i) {
        RoomBorder& wall = room.borders[i];
        wall.startPos = room.offsetFromZero;
        assert(wall.startPos.x >= 0 && wall.startPos.y >= 0);
        wall.endPos = room.offsetFromZero;
        wall.length = 1;
    }
    room.borders[(int)RoomBorderOuterDir::LEFT].startAdjacent = &room.borders[(int)RoomBorderOuterDir::BOTTOM];
    room.borders[(int)RoomBorderOuterDir::LEFT].endAdjacent = &room.borders[(int)RoomBorderOuterDir::TOP];
    room.borders[(int)RoomBorderOuterDir::TOP].startAdjacent = &room.borders[(int)RoomBorderOuterDir::LEFT];
    room.borders[(int)RoomBorderOuterDir::TOP].endAdjacent = &room.borders[(int)RoomBorderOuterDir::RIGHT];
    room.borders[(int)RoomBorderOuterDir::RIGHT].startAdjacent = &room.borders[(int)RoomBorderOuterDir::TOP];
    room.borders[(int)RoomBorderOuterDir::RIGHT].endAdjacent = &room.borders[(int)RoomBorderOuterDir::BOTTOM];
    room.borders[(int)RoomBorderOuterDir::BOTTOM].startAdjacent = &room.borders[(int)RoomBorderOuterDir::RIGHT];
    room.borders[(int)RoomBorderOuterDir::BOTTOM].endAdjacent = &room.borders[(int)RoomBorderOuterDir::LEFT];

    room.borders[(int)RoomBorderOuterDir::LEFT].outerDir = RoomBorderOuterDir::LEFT;
    room.borders[(int)RoomBorderOuterDir::TOP].outerDir = RoomBorderOuterDir::TOP;
    room.borders[(int)RoomBorderOuterDir::RIGHT].outerDir = RoomBorderOuterDir::RIGHT;
    room.borders[(int)RoomBorderOuterDir::BOTTOM].outerDir = RoomBorderOuterDir::BOTTOM;
}

struct DoorBFSNode {
    ui32 index;
};

void placeInteriorDoor(BuildingBlueprint& bp, std::vector<bool>& isConnected, RoomNode& room, RoomNode& adjacentRoom, ui32 tileIndex, const ui32 doorTileIndex, const ui32 outerTileIndex, VisualLog* visLog) {
    if (bp.tiles[outerTileIndex].type <= BlueprintTileType::FLOOR) {
        isConnected[adjacentRoom.id] = true;
        room.adjacentRooms[room.numAdjacentRooms++] = RoomGateInfo{ adjacentRoom.id, outerTileIndex };
        adjacentRoom.adjacentRooms[adjacentRoom.numAdjacentRooms++] = RoomGateInfo{ bp.ownerArray[tileIndex], tileIndex };
        bp.tiles[doorTileIndex].type = BlueprintTileType::DOOR;
        // Visual log
        if (visLog) {
            const f32v3 pos(doorTileIndex % bp.aabb.dims.x, doorTileIndex % (bp.aabb.dims.x * bp.aabb.dims.y) / bp.aabb.dims.x, adjacentRoom.floorIndex * bp.floorHeight);
            visLog->addFilledQuad(pos, f32v2(1.0f), color4(1.0f, 1.0f, 1.0f, 0.8f));
        }
    }
    else if (visLog) {
        const f32v3 pos(doorTileIndex % bp.aabb.dims.x, doorTileIndex % (bp.aabb.dims.x * bp.aabb.dims.y) / bp.aabb.dims.x, adjacentRoom.floorIndex * bp.floorHeight);
        visLog->addFilledQuad(pos, f32v2(1.0f), color4(1.0f, 0.0f, 0.0f, 1.0f));
    }
}

void doorBfs(std::vector<DoorBFSNode>& bfs, size_t& bfsBackIndex, BuildingBlueprint& bp, RoomBorderOuterDir dir, ui32 tileIndex, RoomNode& room, const i16v2& currentPos, std::vector<bool>& visited, std::vector<bool>& isConnected, bool& canConnectToOutside, VisualLog* visLog) {
    const i16v2& directionOffset = EXPAND_OFFSETS[e_cast(dir)];
    const i16v2 nextPos = currentPos + directionOffset;
    const ui32 nextTileIndex = getIndexAtPos(ui32v2(nextPos), bp.aabb.dims, room.floorIndex);
    if (!visited[nextTileIndex]) {
        RoomNodeID nextId = bp.ownerArray[nextTileIndex];
        visited[nextTileIndex] = true;
        if (nextId == bp.ownerArray[tileIndex]) {
            bfs[bfsBackIndex++].index = nextTileIndex;
            if (bfsBackIndex >= bfs.size()) bfsBackIndex = 0;
        }
        else {

            if (nextId == INVALID_ROOM_ID) {
                // Check if this is actually technically an interior wall that is marked as exterior,
                // which can happen if these tiles are unowned but directly between two rooms
                const i16v2 outerPos = nextPos + directionOffset;
                const ui32 outerIndex = getIndexAtPos(ui32v2(outerPos), bp.aabb.dims, room.floorIndex);
                // Check if we are in bounds, if this is a room
                if (boundsCheckTile(outerPos, bp.aabb) && bp.ownerArray[outerIndex] != INVALID_ROOM_ID) {
                    // If not same as previous room
                    if (bp.ownerArray[outerIndex] != bp.ownerArray[tileIndex]) {
                        RoomNode& adjacent = bp.rooms[bp.ownerArray[outerIndex]];
                        if (adjacent.numAdjacentRooms == MAX_ADJACENT_ROOMS) {
                            return;
                        }
                        // TODO: By removing this check we can knock down a whole wall. Kinda cool...
                        if (!isConnected[adjacent.id]) {
                            placeInteriorDoor(bp, isConnected, room, adjacent, tileIndex, nextTileIndex, outerIndex, visLog);
                        }
                    }
                }
                else {
                    // Exterior doors
                    if (canConnectToOutside) {
                        const i16v2 nextNextPos = nextPos + directionOffset;
                        const ui32 nextNextTileIndex = getIndexAtPos(ui32v2(nextNextPos), bp.aabb.dims, room.floorIndex);
                        if (bp.tiles[tileIndex].type == BlueprintTileType::WALL && bp.tiles[nextTileIndex].type == BlueprintTileType::FLOOR) {
                            canConnectToOutside = false;
                            bp.tiles[tileIndex].type = BlueprintTileType::DOOR;
                            bp.exteriorDoors.emplace_back(RoomGateInfo{ bp.ownerArray[tileIndex], tileIndex });
                            // Visual log
                            if (visLog) {
                                const f32v3 pos(tileIndex % bp.aabb.dims.x, tileIndex % (bp.aabb.dims.x * bp.aabb.dims.y) / bp.aabb.dims.x, (tileIndex / (bp.aabb.dims.x * bp.aabb.dims.y)) * bp.floorHeight);
                                visLog->addFilledQuad(pos, f32v2(1.0f), color4(0.0f, 1.0f, 1.0f, 0.8f));
                            }
                        }
                        else if (bp.tiles[nextTileIndex].type == BlueprintTileType::WALL && bp.tiles[tileIndex].type == BlueprintTileType::FLOOR) {
                            // Make sure there isn't a second wall in the way, which can happen at edge of adjacent room
                            if (!boundsCheckTile(nextNextPos, bp.aabb) || bp.tiles[nextNextTileIndex].type != BlueprintTileType::WALL) {
                                canConnectToOutside = false;
                                bp.tiles[nextTileIndex].type = BlueprintTileType::DOOR;
                                bp.exteriorDoors.emplace_back(RoomGateInfo{ bp.ownerArray[tileIndex], tileIndex });
                                // Visual log
                                if (visLog) {
                                    const f32v3 pos(nextTileIndex % bp.aabb.dims.x, nextTileIndex % (bp.aabb.dims.x * bp.aabb.dims.y) / bp.aabb.dims.x, (nextTileIndex / (bp.aabb.dims.x * bp.aabb.dims.y)) * bp.floorHeight);
                                    visLog->addFilledQuad(pos, f32v2(1.0f), color4(1.0f, 0.0f, 1.0f, 0.8f));
                                }
                            }
                        }
                    }
                }
            }
            else if (!isConnected[nextId] && room.numAdjacentRooms < MAX_ADJACENT_ROOMS) {
                // Interior doors
                RoomNode& adjacent = bp.rooms[nextId];
                if (adjacent.numAdjacentRooms < MAX_ADJACENT_ROOMS) {

                    if (bp.tiles[nextTileIndex].type == BlueprintTileType::WALL && bp.tiles[tileIndex].type == BlueprintTileType::FLOOR) {

                        const i16v2 outerPos = nextPos + directionOffset;
                        if (!boundsCheckTile(outerPos, bp.aabb)) {
                            return;
                        }

                        const ui32 outerIndex = getIndexAtPos(ui32v2(outerPos), bp.aabb.dims, room.floorIndex);
                        placeInteriorDoor(bp, isConnected, room, adjacent, tileIndex, nextTileIndex, outerIndex, visLog);
                    }
                }
            }
        }
    }
}

void BuildingBlueprintGenerator::placeDoors(BuildingBlueprint& bp, VisualLog* visLog) {
    if (visLog) visLog->nextStep("Place doors");
    // TODO: Re-use memory
    std::vector<bool> visited(bp.tiles.size());
    std::vector<bool> isConnected(bp.rooms.size());

    // Ringbuffer
    // TODO: Re-use memory
    std::vector<DoorBFSNode> bfs(bp.tiles.size());
    size_t bfsFrontIndex;
    size_t bfsBackIndex;
    bool canConnectToOutside = true;
    for (auto&& room : bp.rooms) {
        assert(room.size);
        // Clear visited list
        std::fill(visited.begin(), visited.end(), 0);
        std::fill(isConnected.begin(), isConnected.end(), 0);

        // Mark neighbor rooms as connected
        for (int i = 0; i < room.numAdjacentRooms; ++i) {
            isConnected[room.adjacentRooms[i].adjacentRoom] = true;
        }

        bfsFrontIndex = 0;
        bfsBackIndex = 1;

        const ui32 startIndex = getIndexAtPos(ui32v2(room.offsetFromZero), bp.aabb.dims, room.floorIndex);
        // Visual log
        if (visLog) {
            visLog->addFilledQuad(f32v3(room.offsetFromZero.x, room.offsetFromZero.y, room.floorIndex * bp.floorHeight), f32v2(1.0f), ROOM_COLORS[room.id % MAX_ROOM_COLORS]);
        }
        visited[startIndex] = true;
        bfs[bfsFrontIndex].index = startIndex;
        const ui32 floorSize = bp.aabb.dims.x * bp.aabb.dims.y;
        // Do the bfs
        while (bfsFrontIndex != bfsBackIndex) {
            const DoorBFSNode& node = bfs[bfsFrontIndex];
            i32v2 pos(node.index % bp.aabb.dims.x, (node.index % floorSize) / bp.aabb.dims.x);
            RoomNodeID roomId = bp.ownerArray[node.index];
            assert(roomId == room.id);
            if (room.numAdjacentRooms == MAX_ADJACENT_ROOMS) {
                break;
            }

            if (visLog) {
                visLog->addWireQuad(f32v3(pos.x, pos.y, room.floorIndex * bp.floorHeight), f32v2(1.0f), color4(0.8f, 0.8f, 0.8f, 0.5f));
            }
            // Left
            if (pos.x > 0) {
                doorBfs(bfs, bfsBackIndex, bp, RoomBorderOuterDir::LEFT, node.index, room, pos, visited, isConnected, canConnectToOutside, visLog);
            }

            // Bottom
            if (pos.y > 0) {
                doorBfs(bfs, bfsBackIndex, bp, RoomBorderOuterDir::BOTTOM, node.index, room, pos, visited, isConnected, canConnectToOutside, visLog);
            }

            // Right
            if (pos.x < bp.aabb.dims.x - 1) {
                doorBfs(bfs, bfsBackIndex, bp, RoomBorderOuterDir::RIGHT, node.index, room, pos, visited, isConnected, canConnectToOutside, visLog);
            }

            // Up
            if (pos.y < bp.aabb.dims.y - 1) {
                doorBfs(bfs, bfsBackIndex, bp, RoomBorderOuterDir::TOP, node.index, room, pos, visited, isConnected, canConnectToOutside, visLog);
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
    const ui32v2 floorDims(bp.aabb.dims.x, bp.aabb.dims.y);
    bits.resize(floorDims.x * floorDims.y);
    for (auto&& room : bp.rooms) {
   
        ui32 tileIndex = room.floorIndex * floorDims.x * floorDims.y;
        for (ui32 y = 0; y < bp.aabb.dims.y; ++y) {
            for (ui32 x = 0; x < bp.aabb.dims.x; ++x) {
                bits.setBitTo(y * floorDims.x + x, bp.ownerArray[tileIndex] == room.id && bp.tiles[tileIndex].type == BlueprintTileType::FLOOR);
                ++tileIndex;
            }
        }
        const ui32v2 dims2d(bp.aabb.dims.x, bp.aabb.dims.y);
        room.interiorEdges = GridEdgeFinder::getInteriorEdgesFromOwnershipArray(bits, dims2d, visLog, room.floorIndex * bp.floorHeight);
        room.edgeWalk = GridEdgeFinder::getInteriorCounterClockwiseWalkFromGridEdges(room.interiorEdges, dims2d);
    }

    if (visLog) {
        visLog->nextStep("Debug interior edges");
        for (auto&& room : bp.rooms) {
            for (auto&& edge : room.interiorEdges) {
                if (edge.edgeDir == Cartesian::SOUTH) {
                    ui32v2 pos = getPosAtIndex(edge.start, bp.aabb.dims);
                    visLog->addWireQuad(f32v3(pos.x, pos.y, room.floorIndex * bp.floorHeight), f32v2(edge.length, 1.0f), color4(0.0f, 1.0f, 1.0f, 0.9f));
                }
                else if (edge.edgeDir == Cartesian::NORTH) {
                    ui32v2 pos = getPosAtIndex(edge.end, bp.aabb.dims);
                    visLog->addWireQuad(f32v3(pos.x, pos.y, room.floorIndex * bp.floorHeight), f32v2(edge.length, 1.0f), color4(0.0f, 1.0f, 0.0f, 0.9f));
                }
                else if (edge.edgeDir == Cartesian::WEST) {
                    ui32v2 pos = getPosAtIndex(edge.end, bp.aabb.dims);
                    visLog->addWireQuad(f32v3(pos.x, pos.y, room.floorIndex * bp.floorHeight), f32v2(1.0f, edge.length), color4(1.0f, 0.0f, 1.0f, 0.9f));
                }
                else if (edge.edgeDir == Cartesian::EAST) {
                    ui32v2 pos = getPosAtIndex(edge.start, bp.aabb.dims);
                    visLog->addWireQuad(f32v3(pos.x, pos.y, room.floorIndex * bp.floorHeight), f32v2(1.0f, edge.length), color4(1.0f, 0.0f, 0.0f, 0.9f));
                }
            }
        }
    }

    if (visLog) {
        visLog->nextStep("Debug edge walk");
        for (auto&& room : bp.rooms) {
            for (auto&& index : room.edgeWalk) {
                ui32v2 pos = getPosAtIndex(index, bp.aabb.dims);
                visLog->addWireQuad(f32v3(pos.x, pos.y, room.floorIndex * bp.floorHeight), f32v2(1.0f), color4(1.0f, 1.0f, 1.0f, 0.9f));
            }
        }
    }

    std::cout << " LOL " << timer.stop() << std::endl;
}

bool tileBlocksDoor(TileIndex index, BuildingBlueprint& bp) {
    const i32v2 pos = getPosAtIndex(index, bp.aabb.dims);
    assert(pos.x > 0 && pos.x < bp.aabb.dims.x - 1 && pos.y > 0 && pos.y < bp.aabb.dims.y - 1); // We should have a wall buffer guarenteed
    if (bp.tiles[index - 1].type == BlueprintTileType::DOOR) {
        return true;
    }
    if (bp.tiles[index - bp.aabb.dims.x].type == BlueprintTileType::DOOR) {
        return true;
    }
    if (bp.tiles[index + 1].type == BlueprintTileType::DOOR) {
        return true;
    }
    if (bp.tiles[index + bp.aabb.dims.x].type == BlueprintTileType::DOOR) {
        return true;
    }
    return false;
}

bool canPlaceStairsHere(TileIndex index, BuildingBlueprint& bp, RoomNode& child) {
    const TileIndex aboveIndex = index + bp.aabb.dims.x * bp.aabb.dims.y;
    if (bp.tiles[index].type == BlueprintTileType::FLOOR &&
        bp.ownerArray[aboveIndex] == child.id &&
        bp.tiles[aboveIndex].type == BlueprintTileType::FLOOR &&
        !tileBlocksDoor(index, bp) && !tileBlocksDoor(aboveIndex, bp)) {
        return true;
    }
    return false;
}

bool isAtWallCorner(TileIndex index, BuildingBlueprint& bp) {
    const ui32v2 pos = getPosAtIndex(index, bp.aabb.dims);
    assert(pos.x > 0 && pos.x < bp.aabb.dims.x - 1 && pos.y > 0 && pos.y < bp.aabb.dims.y - 1); // We should have a wall buffer guarenteed
    ui32 adjacentWallCount = 0;
    if (bp.tiles[index - 1].type == BlueprintTileType::WALL) {
        ++adjacentWallCount;
    }
    if (bp.tiles[index - bp.aabb.dims.x].type == BlueprintTileType::WALL) {
        ++adjacentWallCount;
    }
    if (bp.tiles[index + 1].type == BlueprintTileType::WALL) {
        ++adjacentWallCount;
    }
    if (bp.tiles[index + bp.aabb.dims.x].type == BlueprintTileType::WALL) {
        ++adjacentWallCount;
    }
    // TODO: This disallows single block hallways but meh...
    return adjacentWallCount >= 2;
}

bool isRunningIntoWallAtEnd(TileIndex index, BuildingBlueprint& bp, Cartesian dir) {
    i16v2 pos = getPosAtIndex(index, bp.aabb.dims);
    const ui32 floorIndex = index / (bp.aabb.dims.x * bp.aabb.dims.y);
    assert(pos.x > 0 && pos.x < bp.aabb.dims.x - 1 && pos.y > 0 && pos.y < bp.aabb.dims.y - 1); // We should have a wall buffer guarenteed
    pos += CARTESIAN_NORMALS[e_cast(dir)];
    return bp.tiles[getIndexAtPos(pos, bp.aabb.dims, floorIndex + 1)].type != BlueprintTileType::FLOOR;
}

bool BuildingBlueprintGenerator::placeStairs(BuildingBlueprint& bp, VisualLog* visLog) {
    if (visLog) visLog->nextStep("Place stairs");

    boost::container::static_vector<TileIndex, 256> runs;
    boost::container::static_vector<ui8, 256> runStarts;
    boost::container::static_vector<ui8, 256> runLengths;
    boost::container::static_vector<Cartesian, 256> dirs;
    BitArray usedTiles;
    usedTiles.resize(bp.aabb.dims.x * bp.aabb.dims.y);
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
        ui32 currentRunLength = 0;
        ui32 i = 0;
        while (true) {
            const ui32 bitIndex = room.edgeWalk[i];
            const TileIndex index = bitIndex + room.floorIndex * bp.aabb.dims.x * bp.aabb.dims.y;
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
                    const ui32v2 pos = getPosAtIndex(index, bp.aabb.dims);
                    visLog->addFilledQuad(f32v3(pos.x, pos.y, room.floorIndex * bp.floorHeight), f32v2(1.0f), color4(1.0f, 1.0f, 1.0f, 0.25f));
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
                    const ui32v2 pos = getPosAtIndex(index, bp.aabb.dims);
                    visLog->addWireQuad(f32v3(pos.x, pos.y, room.floorIndex * bp.floorHeight), f32v2(1.0f), color4(0.7f, 0.0f, 0.0f, 0.9f));
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
        const ui32 STAIRS_UP_COUNT = bp.floorHeight + 1;
        ui32 bestRunStart = UINT32_MAX;
        ui32 bestRunLength = UINT32_MAX;
        if (runs.size()) {
            dirs.resize(runs.size());
            for (size_t i = 0; i < runStarts.size(); ++i) {
                const ui32 runStart = runStarts[i];
                const ui32 runLength = runLengths[i];
                if (runLength < STAIRS_UP_COUNT) {
                    continue;
                }

                Cartesian prevDir = Cartesian::NONE;
                Cartesian dir;
                
                ui32 upCount = 0;
                bool wasFlat = false;
                ui32 j;
                for (j = 0; j < runLength - 1; ++j) {
                    // Look ahead for direction
                    const TileIndex index = runs[runStart + j];
                    const i32v2 pos = getPosAtIndex(index, bp.aabb.dims);
                    const i32v2 nextPos = getPosAtIndex(runs[runStart + j + 1], bp.aabb.dims);
                    if (nextPos.x > pos.x) {
                        dir = Cartesian::EAST;
                        assert(prevDir != Cartesian::WEST);
                        if (visLog) {
                            visLog->addFilledQuad(f32v3(pos.x, pos.y, room.floorIndex * bp.floorHeight), f32v2(1.0f), color4(1.0f, 0.0f, 0.0f, 0.5f));
                        }
                    }
                    else if (nextPos.y > pos.y) {
                        dir = Cartesian::NORTH;
                        assert(prevDir != Cartesian::SOUTH);
                        if (visLog) {
                            visLog->addFilledQuad(f32v3(pos.x, pos.y, room.floorIndex * bp.floorHeight), f32v2(1.0f), color4(0.0f, 0.0f, 1.0f, 0.5f));
                        }
                    }
                    else if (nextPos.x < pos.x) {
                        dir = Cartesian::WEST;
                        assert(prevDir != Cartesian::EAST);
                        if (visLog) {
                            visLog->addFilledQuad(f32v3(pos.x, pos.y, room.floorIndex * bp.floorHeight), f32v2(1.0f), color4(1.0f, 1.0f, 0.0f, 0.5f));
                        }
                    }
                    else {
                        dir = Cartesian::SOUTH;
                        assert(prevDir != Cartesian::NORTH);
                        if (visLog) {
                            visLog->addFilledQuad(f32v3(pos.x, pos.y, room.floorIndex * bp.floorHeight), f32v2(1.0f), color4(0.0f, 1.0f, 1.0f, 0.5f));
                        }
                    }
                    // Store dir
                    dirs[runStart + j] = dir;
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
                                        visLog->addWireQuad(f32v3(pos.x, pos.y, room.floorIndex * bp.floorHeight), f32v2(1.0f), color4(1.0f, 0.0f, 0.0f, 1.0f));
                                    }
                                }

                                break;
                            }
                        }
                        if (visLog) {
                            visLog->addWireQuad(f32v3(pos.x, pos.y, room.floorIndex * bp.floorHeight), f32v2(1.0f), color4(1.0f, 1.0f, 1.0f, 1.0f));
                        }
                    }
                    else {
                        if (visLog) {
                            visLog->addWireQuad(f32v3(pos.x, pos.y, room.floorIndex * bp.floorHeight), f32v2(1.0f), color4(0.0f, 0.0f, 1.0f, 1.0f));
                        }
                    }

                    prevDir = dir;
                }
                // Last one is up always (if not into a wall)
                if (upCount == STAIRS_UP_COUNT - 1) {
                    // If running into a wall, not done
                    if (!isRunningIntoWallAtEnd(runs[runStart + j], bp, dir)) {
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
        if (bestRunStart == UINT32_MAX) {
            continue; // No run found! TODO: Ladder?
        }
        // Add stair pieces
        room.stairs.reserve(room.stairs.size() + bestRunLength);
        ui32 height = 0;
        Cartesian prevDir = Cartesian::NONE;
        for (ui32 j = 0; j < bestRunLength; ++j) {
            const TileIndex tileIndex = runs[bestRunStart + j];
            bp.tiles[tileIndex].type = BlueprintTileType::STAIRS;
            if (j > 0) {
                bp.tiles[tileIndex + bp.aabb.dims.x * bp.aabb.dims.y].type = BlueprintTileType::AIR;
            }
            StairPiece& stairPiece = room.stairs.emplace_back(StairPiece{});
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
                const i32v2 pos = getPosAtIndex(runs[bestRunStart + j], bp.aabb.dims);
                visLog->addFilledQuad(f32v3(pos.x, pos.y, room.floorIndex * bp.floorHeight), f32v2(1.0f), color4(0.0f, 1.0f, 0.0f, 0.9f));
            }
        }
    }
    return true;
}

void BuildingBlueprintGenerator::postProcessBlueprint(BuildingBlueprint& bp) {
    // Tally required items
    std::map<ItemID, ui32> requiredItems;

    bp.tileRecipes[e_cast(BlueprintTileType::NONE)] = nullptr;
    bp.tileRecipes[e_cast(BlueprintTileType::FLOOR)] = &TileRepository::getTileData(bp.tileIDs[e_cast(BlueprintTileType::FLOOR)]).recipe;
    bp.tileRecipes[e_cast(BlueprintTileType::DOOR)] = &TileRepository::getTileData(bp.tileIDs[e_cast(BlueprintTileType::DOOR)]).recipe;
    bp.tileRecipes[e_cast(BlueprintTileType::WALL)] = &TileRepository::getTileData(bp.tileIDs[e_cast(BlueprintTileType::WALL)]).recipe;
    bp.tileRecipes[e_cast(BlueprintTileType::STAIRS)] = nullptr;
    bp.tileRecipes[e_cast(BlueprintTileType::AIR)] = nullptr;
    static_assert(e_cast(BlueprintTileType::TYPES) == 6);

    std::unordered_map<RoomNodeID, ui32v4 /* xspan, yspan */ > roomBoundsLookup;
    roomBoundsLookup.reserve(20);

    for (ui32 i = 0; i < (ui32)bp.tiles.size(); ++i) {
        // Compute bounds
        RoomNodeID id = bp.ownerArray[i];
        if (id != INVALID_ROOM_ID) {
            const ui32v2 pos = getPosAtIndex(i, bp.aabb.dims);
            auto&& it = roomBoundsLookup.find(id);
            if (it == roomBoundsLookup.end()) {
                roomBoundsLookup[id] = ui32v4(pos.x, pos.x, pos.y, pos.y);
            }
            else {
                // AABB bounds
                if (pos.x < it->second.x) {
                    it->second.x = pos.x;
                } else if (pos.x > it->second.y) {
                    it->second.y = pos.x;
                }
                if (pos.y < it->second.z) {
                    it->second.z = pos.y;
                } else if (pos.y > it->second.w) {
                    it->second.w = pos.y;
                }
            }
        }
        // Tile postprocess
        switch (bp.tiles[i].type) {
            case BlueprintTileType::NONE:
            case BlueprintTileType::AIR:
            case BlueprintTileType::STAIRS: // TODO: Not stairs
                bp.tiles[i].isBuilt = true;
                break;
            case BlueprintTileType::WALL:
            case BlueprintTileType::FLOOR:
            case BlueprintTileType::DOOR:

                ++bp.totalTilesToBuild;
                for (auto&& itemStack : *bp.tileRecipes[e_cast(bp.tiles[i].type)]) {
                    auto&& it = requiredItems.find(itemStack.id);
                    if (it == requiredItems.end()) {
                        requiredItems[itemStack.id] = itemStack.quantity;
                    }
                    else {
                        it->second += itemStack.quantity;
                    }
                }
                break;
            case BlueprintTileType::TYPES:
            default:
                assert(false);
                break;
        }
    }
    static_assert(e_cast(BlueprintTileType::TYPES) == 6);

    // Set up AABBs
    for (auto&& it : roomBoundsLookup) {
        RoomNode& room = bp.rooms[it.first];
        room.aabb.pos.x = it.second.x + bp.aabb.x;
        room.aabb.dims.x = it.second.y - it.second.x + 1;
        room.aabb.pos.y = it.second.z + bp.aabb.y;
        room.aabb.dims.y = it.second.w - it.second.z + 1;
    }

    for (auto&& it : requiredItems) {
        bp.requiredItemsToBuild.push_back(ItemStackUnbounded{ it.first, it.second });
    }
}

BuildingBlueprintId BuildingBlueprintGenerator::getNextBuildingID() {
    ++sCurrentId;
    // Will this ever happen? maybe...
    if (sCurrentId == INVALID_BLUEPRINT_ID) {
        sCurrentId = 0;
    }
    return sCurrentId;
}
