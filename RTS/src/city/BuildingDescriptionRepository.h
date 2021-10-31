#pragma once

#include "Building.h"

DECL_VIO(class IOManager);

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
