#pragma once

// TODO: PCH?

#include "CityConst.h"
#include "city/BuildingGrammar.h"
#include "item/ItemTradeManager.h"

#include "city/CityPlot.h"

DECL_VIO(class IOManager);
class Building;

// TODO: Move to data?
enum class BuildingFunction : ui16 {
    NONE,
    RESIDENCE,
    LUMBERMILL,
    TYPES
};
KEG_ENUM_DECL(BuildingFunction);

struct RoomDescription {
    RoomTypeID typeID;
    f32 minWidth;
    f32 maxWidth;
    f32 desiredAspectRatio; // Width / Height
};

// TODO: Can we optimize passing this around so theres no copies?
class Building {
public:
    // Building bounds are a series of corner segments
    ui32v2 mBottomLeftWorldPos;
    std::vector<RoomNode> mGraph;
    CityPlotIndex mPlotIndex = INVALID_PLOT_INDEX;
    BuildingFunction mFunction = BuildingFunction::NONE;
    BuildingID mId;

    // TODO: Move to Business?
    //ItemTradeManager mTradeManager; // TODO: This is a large copy and we pass building by value
};

enum class RoadType {
    DIRT,
    PAVED
};

struct CityRoad {
    ui32v2 startPos;
    ui32v2 endPos;
    ui32v4 aabb;
    ui32 width;
    RoadType type = RoadType::PAVED;
    RoadID id;
    std::vector<RoadID> neighborRoads;
    bool mIsBuilt = false;
};

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
    ui32v2 employeeCountRange = ui32v2(0);
    f32 minAspectRatio = 0.5f;
    BuildingGrammar publicGrammar;
    BuildingFunction function = BuildingFunction::NONE;
    // TODO: More cache friendly combination?
    std::vector<PossibleRoom> publicRooms;
    std::vector<PossibleRoom> privateRooms;
    std::vector<PossibleSubRoom> subRooms;
};


//struct Facade {
//
//};

class BuildingDescriptionRepository {
public:
    BuildingDescriptionRepository(vio::IOManager& ioManager);

    void loadRoomDescriptionFile(const vio::Path& filePath);
    void loadBuildingDescriptionFile(const vio::Path& filePath);

    BuildingDescription& getBuildingDescription(const nString& name);
    RoomDescription& getRoomDescriptionFromID(RoomTypeID id);
    const nString* getNameFromRoomTypeID(RoomTypeID id);

private:
    // TODO: HashedString?
    std::map<nString, RoomTypeID> mRoomTypes;
    std::vector<RoomDescription> mRoomDescriptions; // Key is RoomTypeID

    std::map<nString, BuildingTypeID> mBuildingTypes;
    std::vector<BuildingDescription> mBuildingDescriptions; // Key is BuildingTypeID

    vio::IOManager& mIoManager;
};
