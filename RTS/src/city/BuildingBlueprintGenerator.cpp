#include "stdafx.h"
#include "BuildingBlueprintGenerator.h"

#include "Random.h"

std::unique_ptr<BuildingBlueprint> BuildingBlueprintGenerator::generateBuilding(const BuildingDescription& desc, float sizeAlpha)
{
    std::unique_ptr<BuildingBlueprint> bp = std::make_unique<BuildingBlueprint>(desc, sizeAlpha);

    // Generate floorplan size
    bp->dims.x = vmath::lerp(desc.widthRange.x, desc.widthRange.y, sizeAlpha);
    // TODO: Dynamic aspect ratio
    bp->dims.y = bp->dims.x * desc.minAspectRatio;

    addPublicRoomsToGraph(*bp);
    assert(desc.publicRoomCountRange.y != 0.0f);
    assignPublicRooms(*bp);
    addPrivateRoomsToGraph(*bp);
    placeRooms(*bp);
    expandRooms(*bp);
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

void placeChildrenRecursive(std::vector<RoomNode>& nodes, RoomNode* node, f32 availableYSpan, ui16 xOffsetPerLayer, i16v2 currentOffset) {
    if (node->numChildren == 0) {
        return;
    }

    f32 childYSpan = availableYSpan / node->numChildren;
    currentOffset.x += xOffsetPerLayer;
    f32 ySegmentSize = availableYSpan / (node->numChildren * 2);
    // Start at the top
    currentOffset.y -= ySegmentSize * (node->numChildren - 1);
    for (int i = 0; i < node->numChildren; ++i) {
        RoomNode& child = nodes[node->childRooms[i]];
        child.offsetFromRoot = currentOffset;
        placeChildrenRecursive(nodes, &child, childYSpan, xOffsetPerLayer, currentOffset);
        currentOffset.y += ySegmentSize * 2;
    }
}

void BuildingBlueprintGenerator::placeRooms(BuildingBlueprint& bp) {
    // Breadth first search room placement
    RoomNode* root = &bp.nodes[0];
    root->offsetFromRoot = i16v2(0, 0);

    ui16 maximumDepth = getMaximumDepthRecursive(bp.nodes, root);

    ui16 xOffsetPerLayer = bp.dims.x / maximumDepth;
    f32 availableYSpan = bp.dims.y;
    // First place the root
    root->offsetFromRoot = i16v2(xOffsetPerLayer / 2, 0);

    placeChildrenRecursive(bp.nodes, root, availableYSpan, xOffsetPerLayer, root->offsetFromRoot);
}

void BuildingBlueprintGenerator::expandRooms(BuildingBlueprint& bp) {
    bp.tiles.resize((size_t)bp.dims.x * (size_t)bp.dims.y, {0, BlueprintTileType::FLOOR });
}
