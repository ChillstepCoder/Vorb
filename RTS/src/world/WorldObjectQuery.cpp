#include "stdafx.h"
#include "WorldObjectQuery.h"

WorldObjectQuery::WorldObjectQuery(World& world, ui32v2& tilePos) :
    mWorld(world),
    mTilePos(tilePos)
{
    refresh();
}

void WorldObjectQuery::refresh() {

}
