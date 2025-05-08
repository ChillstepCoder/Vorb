#pragma once

//#include "building/building.h"
#include "definitions/BuildingDef.h"
#include "resources/DataAssetRepository.h"

class BuildingRepository : public DataAssetRepository<BuildingDef, AssetType::Building, "bldg"> {
public:
    using DataAssetRepository<BuildingDef, AssetType::Building, "bldg">::DataAssetRepository;
    static void initInstance(vio::IOManager& ioManager) {
        IAssetRepository<BuildingDef>::sInstance = std::make_unique<BuildingRepository>(ioManager);
    }
    inline static BuildingRepository& get() {
        assert(IAssetRepository<BuildingDef>::sInstance);
        return (BuildingRepository&)*IAssetRepository<BuildingDef>::sInstance;
    }

    void onAllAssetTypesRegistered() override;
};

class RoomRepository : public DataAssetRepository<RoomDef, AssetType::Room, "room"> {
public:
    using DataAssetRepository<RoomDef, AssetType::Room, "room">::DataAssetRepository;
    static void initInstance(vio::IOManager& ioManager) {
        IAssetRepository<RoomDef>::sInstance = std::make_unique<RoomRepository>(ioManager);
    }
    inline static RoomRepository& get() {
        assert(IAssetRepository<RoomDef>::sInstance);
        return (RoomRepository&)*IAssetRepository<RoomDef>::sInstance;
    }
};
