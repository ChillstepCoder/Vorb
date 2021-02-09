#pragma once

// TODO: Roomdetials file?
constexpr ui32 MAX_CHILD_ROOMS = 4;
constexpr ui32 MAX_ADJACENT_ROOMS = 2;
constexpr ui32 MAX_WALLS_PER_ROOM = 8;
static_assert(MAX_WALLS_PER_ROOM % 2 == 0, "Should be even");

// Constants
constexpr ui8 INVALID_ROOM_ID = UINT8_MAX;
typedef ui8 RoomNodeID;

// Types
typedef ui16 RoomTypeID;
typedef ui16 BuildingTypeID;