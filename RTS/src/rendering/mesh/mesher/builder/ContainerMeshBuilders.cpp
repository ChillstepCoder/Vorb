#include "stdafx.h"
#include "rendering/mesh/mesher/builder/ContainerMeshBuilders.h"
#include "tile/TileContainer.h"

ContainerMeshBuilders::ContainerMeshBuilders(const TileContainer& container, bool staticMeshIsOnlyQuads) :
    container(container),
    staticBuilder(staticMeshIsOnlyQuads),
    dynamicBuilder(false),
    billboardBuilder(),
    modelGatherer(container.getId(), f32v3(container.getTileSpatialGrid().getWorldPos3D()))
{
    container.copyDataWorkerThread(tileData);
    materialDependencies.reserve(32); // Arbitrary
}

void ContainerMeshBuilders::computeBoundingSpheres() {
    // TODO: Share largest?
    staticBuilder.computeBoundingSphere();
    dynamicBuilder.computeBoundingSphere();
    billboardBuilder.computeBoundingSphere();
}
