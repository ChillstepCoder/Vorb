#pragma once

// TODO: PCH?
#include <Vorb/io/Path.h>

#include "city/BuildingGrammar.h"

DECL_VIO(class IOManager);

struct RoomDescription {
    RoomTypeID typeID;
    f32 minWidth;
    f32 maxWidth;
    f32 desiredAspectRatio; // Width / Height
};

// TODO: Can we optimize passing this around so theres no copies? Does it matter...? no
class Building {
public:
    // Building bounds are a series of corner segments
    // Each pair is an X,Y offset pair representing a wall "corner" so this must be even in length
    std::vector<i16v2> mWallSegmentOffsets; // Facade // TODO: This only needs to be i16 since we can interleave x,y
    ui32v2 mBottomLeftWorldPos;
    std::vector<RoomNode> mGraph;
};

// TODO: Move to data
// Buildings can come in various shapes and sizes, and can be entered
//enum class BuildingType : ui16 {
//    HOVEL,
//    HOUSE,
//    BARN,
//    MILL,
//    GROCERY,
//    BLACKSMITH,
//    TAILOR,
//    TAVERN,
//    STOCKPILE,
//    GRANARY,
//    KEEP,
//    MANSION,
//    BARRACKS,
//    SHERIFFS_OFFICE,
//    STABLE,
//};

struct PossibleSubRoom {
    RoomTypeID id;
    ui32v2 countRange = ui32v2(0, 1);
    std::vector<RoomTypeID> parentRoomIDs;
};
struct PossibleRoom {
    RoomTypeID id;
    ui32v2 countRange;
    f32 weight;
};
struct BuildingDescription {
    ui32v2 widthRange = f32v2(10, 30);
    ui32v2 publicRoomCountRange = ui32v2(1, 3);
    ui32v2 privateRoomCountRange = ui32v2(1, 3);
    f32 minAspectRatio = 0.5f;
    BuildingGrammar publicGrammar;
    // TODO: More cache friendly combination?
    std::vector<PossibleRoom> publicRooms;
    std::vector<PossibleRoom> privateRooms;
    std::vector<PossibleSubRoom> subRooms;
};


//struct Facade {
//
//};



struct PlannedBuilding {
    Building mBuilding;
};

class BuildingDescriptionRepository {
public:
    BuildingDescriptionRepository(vio::IOManager& ioManager);

    void loadRoomDescriptionFile(const vio::Path& filePath);
    void loadBuildingDescriptionFile(const vio::Path& filePath);

    BuildingDescription& getBuildingDescription(const nString& name);
    const nString* getNameFromRoomTypeID(RoomTypeID id);

private:
    // TODO: HashedString?
    std::map<nString, RoomTypeID> mRoomTypes;
    std::vector<RoomDescription> mRoomDescriptions; // Key is RoomTypeID

    std::map<nString, BuildingTypeID> mBuildingTypes;
    std::vector<BuildingDescription> mBuildingDescriptions; // Key is BuildingTypeID

    vio::IOManager& mIoManager;
};
