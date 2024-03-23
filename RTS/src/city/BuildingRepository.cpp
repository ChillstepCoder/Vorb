#include "stdafx.h"
#include "BuildingRepository.h"

#include <Vorb/io/IOManager.h>

void BuildingRepository::onAllAssetTypesRegistered() {
    // Grab material IDs
    for (AssetID id = 0; id < mAssets.size(); ++id) {
        BuildingDef& def = *mAssets[id];

        def.publicRooms.resize(def.publicRoomFileData.size());
        for (size_t i = 0; i < def.publicRoomFileData.size(); ++i) {
            PossibleRoomFileData& roomFileData = def.publicRoomFileData[i];
            PossibleRoom& newRoom = def.publicRooms[i];
            StrToken token(roomFileData.name);
            newRoom.id = RoomRepository::getInstance().tryGetRegisteredAssetID(roomFileData.name);
            if (newRoom.id == INVALID_ASSET_ID) {
                panic("Invalid public room def {} on building {}", roomFileData.name, def.getName());
            }
            newRoom.countRange = roomFileData.countRange;
            newRoom.weight = roomFileData.weight;
        }
        def.privateRooms.resize(def.privateRoomFileData.size());
        for (size_t i = 0; i < def.privateRoomFileData.size(); ++i) {
            PossibleRoomFileData& roomFileData = def.privateRoomFileData[i];
            PossibleRoom& newRoom = def.privateRooms[i];
            StrToken token(roomFileData.name);
            newRoom.id = RoomRepository::getInstance().tryGetRegisteredAssetID(roomFileData.name);
            if (newRoom.id == INVALID_ASSET_ID) {
                panic("Invalid private room def {} on building {}", roomFileData.name, def.getName());
            }
            newRoom.countRange = roomFileData.countRange;
            newRoom.weight = roomFileData.weight;
        }
        def.subRooms.resize(def.subRoomFileData.size());
        for (size_t i = 0; i < def.subRoomFileData.size(); ++i) {
            PossibleSubRoomFileData& roomFileData = def.subRoomFileData[i];
            PossibleSubRoom& newRoom = def.subRooms[i];
            StrToken token(roomFileData.name);
            newRoom.id = RoomRepository::getInstance().tryGetRegisteredAssetID(roomFileData.name);
            if (newRoom.id == INVALID_ASSET_ID) {
                panic("Invalid subroom def {} on building {}", roomFileData.name, def.getName());
            }
            newRoom.countRange = roomFileData.countRange;
            newRoom.parentRoomIDs.resize(roomFileData.parentRooms.size());
            for (size_t j = 0; j < roomFileData.parentRooms.size(); ++j) {
                StrToken parentName = roomFileData.parentRooms[j];
                newRoom.parentRoomIDs[j] = RoomRepository::getInstance().tryGetRegisteredAssetID(parentName);
                if (newRoom.parentRoomIDs[j] == INVALID_ASSET_ID) {
                    panic("Invalid subroom parent room def {} on building {}", parentName, def.getName());
                }
            }
        }

        def.publicGrammar.buildFromStrings(def.publicRoomGrammarStrings);
    }
}
