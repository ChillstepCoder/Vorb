#include "stdafx.h"
#include "Building.h"

#include <Vorb/io/IOManager.h>

// Used to parse into a RoomDescription
struct RoomDescriptionFileData {
    f32 minWidth;
    f32 maxWidth;
    f32 desiredAspectRatio = 1.0f; // Width / Height
};
// TODO: Furniture and shit? Purpose?
KEG_TYPE_DEF_SAME_NAME(RoomDescriptionFileData, kt) {
    kt.addValue("min_width", keg::Value::basic(offsetof(RoomDescriptionFileData, minWidth), keg::BasicType::F32));
    kt.addValue("max_width", keg::Value::basic(offsetof(RoomDescriptionFileData, maxWidth), keg::BasicType::F32));
    kt.addValue("aspect_ratio", keg::Value::basic(offsetof(RoomDescriptionFileData, desiredAspectRatio), keg::BasicType::F32));
}

struct PossibleRoomFileData {
    nString name;
    ui32v2 countRange = ui32v2(1, 100);
    f32 weight = 1.0f;
};
KEG_TYPE_DEF_SAME_NAME(PossibleRoomFileData, kt) {
    kt.addValue("name", keg::Value::basic(offsetof(PossibleRoomFileData, name), keg::BasicType::STRING));
    kt.addValue("count_range", keg::Value::basic(offsetof(PossibleRoomFileData, countRange), keg::BasicType::UI32_V2));
    kt.addValue("weight", keg::Value::basic(offsetof(PossibleRoomFileData, weight), keg::BasicType::F32));
}

// Bathrooms, closets, ect
struct PossibleSubRoomFileData {
    nString name;
    ui32v2 countRange = ui32v2(0, 1);
    Array<nString> parentRooms;
};
KEG_TYPE_DEF_SAME_NAME(PossibleSubRoomFileData, kt) {
    kt.addValue("name", keg::Value::basic(offsetof(PossibleSubRoomFileData, name), keg::BasicType::STRING));
    kt.addValue("count_range", keg::Value::basic(offsetof(PossibleSubRoomFileData, countRange), keg::BasicType::UI32_V2));
    kt.addValue("public_rooms", keg::Value::array(offsetof(PossibleSubRoomFileData, parentRooms), keg::BasicType::STRING));
}

struct BuildingDescriptionFileData {
    ui32v2 widthRange = f32v2(10, 30);
    ui32v2 publicRoomCountRange = ui32v2(1, 3);
    ui32v2 privateRoomCountRange = ui32v2(1, 3);
    f32 minAspectRatio = 0.5f;
    Array<nString> publicRoomGrammars;
    Array<PossibleRoomFileData> publicRooms;
    Array<PossibleRoomFileData> privateRooms;
    Array<PossibleSubRoomFileData> subRooms;
};
KEG_TYPE_DEF_SAME_NAME(BuildingDescriptionFileData, kt) {
    kt.addValue("width_range", keg::Value::basic(offsetof(BuildingDescriptionFileData, widthRange), keg::BasicType::UI32_V2));
    kt.addValue("public_room_count_range", keg::Value::basic(offsetof(BuildingDescriptionFileData, publicRoomCountRange), keg::BasicType::UI32_V2));
    kt.addValue("private_room_count_range", keg::Value::basic(offsetof(BuildingDescriptionFileData, privateRoomCountRange), keg::BasicType::UI32_V2));
    kt.addValue("min_aspect_ratio", keg::Value::basic(offsetof(BuildingDescriptionFileData, minAspectRatio), keg::BasicType::F32));
    kt.addValue("public_room_grammars", keg::Value::array(offsetof(BuildingDescriptionFileData, publicRoomGrammars), keg::BasicType::STRING));
    kt.addValue("public_rooms", keg::Value::array(offsetof(BuildingDescriptionFileData, publicRooms), keg::Value::custom(0, "PossibleRoomFileData", false)));
    kt.addValue("private_rooms", keg::Value::array(offsetof(BuildingDescriptionFileData, privateRooms), keg::Value::custom(0, "PossibleRoomFileData", false)));
    kt.addValue("sub_rooms", keg::Value::array(offsetof(BuildingDescriptionFileData, subRooms), keg::Value::custom(0, "PossibleSubRoomFileData", false)));
}
BuildingDescriptionRepository::BuildingDescriptionRepository(vio::IOManager& ioManager) 
    : mIoManager(ioManager)
{

}

void BuildingDescriptionRepository::loadRoomDescriptionFile(const vio::Path& filePath)
{
    if (mIoManager.parseFileAsKegObjectMap(filePath, makeFunctor([&](Sender s, const nString& key, keg::Node value) {
        keg::ReadContext& readContext = *((keg::ReadContext*)s);
        
        RoomDescriptionFileData fileData;
        // Load data
        keg::parse((ui8*)&fileData, value, readContext, &KEG_GLOBAL_TYPE(RoomDescriptionFileData));

        RoomDescription description;
        description.minWidth = fileData.minWidth;
        description.maxWidth = fileData.maxWidth;
        description.desiredAspectRatio = fileData.desiredAspectRatio;

        assert(mRoomTypes.find(key) == mRoomTypes.end());
        RoomTypeID newID = static_cast<RoomTypeID>(mRoomDescriptions.size());
        description.typeID = newID;
        mRoomTypes[key] = newID;
        mRoomDescriptions.emplace_back(std::move(description));
    })));
}

void BuildingDescriptionRepository::loadBuildingDescriptionFile(const vio::Path& filePath)
{
    if (mIoManager.parseFileAsKegObjectMap(filePath, makeFunctor([&](Sender s, const nString& key, keg::Node value) {
        keg::ReadContext& readContext = *((keg::ReadContext*)s);

        BuildingDescriptionFileData fileData;
        // Load data
        keg::parse((ui8*)&fileData, value, readContext, &KEG_GLOBAL_TYPE(BuildingDescriptionFileData));

        BuildingDescription description;
        description.widthRange = fileData.widthRange;
        description.publicRoomCountRange = fileData.publicRoomCountRange;
        description.privateRoomCountRange = fileData.privateRoomCountRange;
        description.minAspectRatio = fileData.minAspectRatio;
        description.publicRooms.reserve(fileData.publicRooms.size());
        // TODO: We should get the keys from this somehow
        for (size_t i = 0; i < fileData.publicRooms.size(); ++i) {
            PossibleRoomFileData& roomFileData = fileData.publicRooms[i];
            PossibleRoom newRoom;
            auto&& it = mRoomTypes.find(roomFileData.name);
            assert(it != mRoomTypes.end());
            newRoom.id = it->second;
            newRoom.countRange = roomFileData.countRange;
            newRoom.weight = roomFileData.weight;
            description.publicRooms.emplace_back(std::move(newRoom));
        }
        description.privateRooms.reserve(fileData.privateRooms.size());
        for (size_t i = 0; i < fileData.privateRooms.size(); ++i) {
            PossibleRoomFileData& roomFileData = fileData.privateRooms[i];
            PossibleRoom newRoom;
            auto&& it = mRoomTypes.find(roomFileData.name);
            assert(it != mRoomTypes.end());
            newRoom.id = it->second;
            newRoom.countRange = roomFileData.countRange;
            newRoom.weight = roomFileData.weight;
            description.privateRooms.emplace_back(std::move(newRoom));
        }
        description.subRooms.reserve(fileData.subRooms.size());
        for (size_t i = 0; i < fileData.subRooms.size(); ++i) {
            PossibleSubRoomFileData& roomFileData = fileData.subRooms[i];
            PossibleSubRoom newRoom;
            auto&& it = mRoomTypes.find(roomFileData.name);
            assert(it != mRoomTypes.end());
            newRoom.id = it->second;
            newRoom.countRange = roomFileData.countRange;
            newRoom.parentRoomIDs.reserve(roomFileData.parentRooms.size());
            for (size_t i = 0; i < roomFileData.parentRooms.size(); ++i) {
                nString& parentName = roomFileData.parentRooms[i];
                auto&& it2 = mRoomTypes.find(parentName);
                assert(it2 != mRoomTypes.end());
                newRoom.parentRoomIDs.emplace_back(it2->second);
            }
        }

        description.publicGrammar.buildFromStrings(fileData.publicRoomGrammars);

        BuildingTypeID newID = static_cast<RoomTypeID>(mBuildingDescriptions.size());

        mBuildingTypes[key] = newID;
        mBuildingDescriptions.emplace_back(std::move(description));
    })));
}

BuildingDescription& BuildingDescriptionRepository::getBuildingDescription(const nString& name)
{
    auto&& it = mBuildingTypes.find(name);
    assert(it != mBuildingTypes.end());
    BuildingTypeID id = it->second;
    return mBuildingDescriptions[id];
}

const nString* BuildingDescriptionRepository::getNameFromRoomTypeID(RoomTypeID id)
{
    for (auto&& it : mRoomTypes) {
        if (it.second == id) {
            return &it.first;
        }
    }
    return nullptr;
}
