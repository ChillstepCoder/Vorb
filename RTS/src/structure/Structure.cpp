#include "stdafx.h"
#include "Structure.h"

#include "world/World.h"
#include "tile/TileContainerRepository.h"

void Structure::freeData() {
    ASSERT_GAME_THREAD();
    if (mTileContainer) {
        mTileContainer->getWorld().getTileContainerRepository().destroyTileContainer(mTileContainer);
        mTileContainer = nullptr;
    }
}
