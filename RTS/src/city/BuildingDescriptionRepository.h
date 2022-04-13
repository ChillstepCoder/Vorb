#pragma once

#include "Building.h"
#include "definitions/BuildingDef.h"

DECL_VIO(class IOManager);

class BuildingDescriptionRepository {
public:
    BuildingDescriptionRepository(vio::IOManager& ioManager);

    void loadRoomDescriptionFile(const vio::Path& filePath);
    void loadBuildingDescriptionFile(const vio::Path& filePath);

    const BuildingDef& getBuildingDef(const nString& name) const;
    const BuildingDef& getBuildingDef(BuildingTypeID id) const { return mBuildingDescriptions[id]; }
    const RoomDef& getRoomDefFromID(RoomDefID id) const;
    const nString* getNameFromRoomDefID(RoomDefID id) const;

    const std::vector<BuildingDef>& getBuildingDefs() const { return mBuildingDescriptions; }

private:
    // TODO: HashedString?
    std::map<nString, RoomDefID> mRoomTypes;
    std::vector<RoomDef> mRoomDefs; // Key is RoomTypeID

    std::map<nString, BuildingTypeID> mBuildingTypes;
    std::vector<BuildingDef> mBuildingDescriptions; // Key is BuildingTypeID

    vio::IOManager& mIoManager;
};
