#include "stdafx.h"
#include "rendering/mesh/mesher/builder/ContainerMeshBuilders.h"
#include "tile/TileContainer.h"

ContainerMeshBuilders::ContainerMeshBuilders(const TileContainer& container, bool staticMeshIsOnlyQuads) :
    staticBuilder(staticMeshIsOnlyQuads),
    dynamicBuilder(false),
    billboardBuilder(),
    modelGatherer(container.getId(), f32v3(container.getTileSpatialGrid().getWorldPos())),
    world(container.getWorld())
{
    containerId = container.getId();
    container.copyDataWorkerThread(tileData);
    materialDependencies.reserve(32); // Arbitrary
}

void ContainerMeshBuilders::computeBoundingSpheres() {
    // TODO: Share largest?
    staticBuilder.computeBoundingSphere();
    dynamicBuilder.computeBoundingSphere();
    billboardBuilder.computeBoundingSphere();
}
