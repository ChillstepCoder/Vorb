#pragma once

#include "rendering/mesh/mesher/builder/ProceduralMeshBuilder.h"
#include "rendering/mesh/mesher/builder/BillboardMeshBuilder.h"
#include "rendering/model/InstancedStaticModelGatherer.h"
#include "tile/Tile.h"

class TileContainer;

class ContainerMeshBuilders
{
public:
    VORB_NON_COPYABLE_BUT_MOVABLE(ContainerMeshBuilders);

    ContainerMeshBuilders(const TileContainer& container, bool staticMeshIsOnlyQuads);
    void computeBoundingSpheres();

    const TileContainer& container;
    ContainerMeshDataCopy tileData;
    ProceduralMeshBuilder staticBuilder;
    ProceduralMeshBuilder dynamicBuilder;
    BillboardMeshBuilder billboardBuilder;
    InstancedStaticModelGatherer modelGatherer;
};

