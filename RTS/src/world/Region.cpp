#include "stdafx.h"
#include "Region.h"

RegionID::RegionID(ui32 id) :
    id(id) {
    pos.x = id % WorldData::WORLD_WIDTH_REGIONS;
    pos.y = id / WorldData::WORLD_WIDTH_REGIONS;

}

RegionID::RegionID(const f32v2 worldPos)
{
    assert(worldPos.x >= 0.0f && worldPos.y >= 0.0f);
    pos = i32v2(floor(worldPos.x / WorldData::REGION_WIDTH_TILES), floor(worldPos.y / WorldData::REGION_WIDTH_TILES));
    id = pos.y * WorldData::WORLD_WIDTH_REGIONS + pos.x;
}
