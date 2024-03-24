#pragma once

struct BuildingBlueprintTileTarget {
    TileIndex tileIndex;
    TileID id;
    //ui8 runLength // TODO: RLE Compression
};

struct BuildingBlueprintWallTarget {
    TileID id;
    Cartesian dir;
    // ui8 runLength // TODO: RLE Compression
};

// Minimal data representation of a building
class BuildingBlueprint {
public:
    BitArray ownedDTiles;
    // Sorted by build priority back to front, so first floor tiles at the end
    std::unique_ptr<BuildingBlueprintTileTarget[]> tileTargets;
    // Sorted by build priority back to front, so first floor walls at the end
    std::unique_ptr<BuildingBlueprintWallTarget[]> wallTargets;
    // Required items to build
    std::unique_ptr<ItemStack[]> itemComposition; // TODO: Maybe this should be flexible... maybe we dont care what items are used? Room specific tiles? ect.
    i32 itemCompositionCount;
    i32 wallTargetCount;
    i32 tileTargetCount;
    DTileCoord worldPosRootDTile;
    i32v2 dimsDTile;
    i32 floorCount;
};
//SIZER(BuildingBlueprint);

typedef std::unique_ptr<BuildingBlueprint> BuildingBlueprintPtr;