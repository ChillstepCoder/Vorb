#pragma once

// TODO: Roomdetials file?
constexpr ui32 MAX_CHILD_ROOMS = 4;
constexpr ui32 MAX_ADJACENT_ROOMS = 3;
constexpr ui32 MAX_WALLS_PER_ROOM = 4;

// Constants
constexpr ui8 INVALID_ROOM_ID = UINT8_MAX;
typedef ui8 RoomNodeID;

typedef ui32 RoadID;
typedef ui32 BuildingID;
#define INVALID_ROAD_ID UINT32_MAX
#define INVALID_BUILDING_ID UINT32_MAX

// Types
typedef ui16 RoomDefID;
typedef ui16 BuildingTypeID;

// TODO: Data driven
// TODO: District Conversion
// Higher numbers are higher priority. For example, Industrial can replace Rural
enum class DistrictType {
    Rural,
    Farming,
    Outpost,
    Residential,
    Industrial,
    Commercial,
    Military,
    Government,
    Harbor,
    Types,
    NONE = Types
};