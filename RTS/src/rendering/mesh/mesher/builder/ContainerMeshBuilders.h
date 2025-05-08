#pragma once

#include "rendering/mesh/mesher/builder/ProceduralMeshBuilder.h"
#include "rendering/mesh/mesher/builder/BillboardMeshBuilder.h"
#include "rendering/model/InstancedStaticModelGatherer.h"
#include "tile/Tile.h"

class TileContainer;
class World;

class ContainerMeshBuilders {
public:
    VORB_NON_COPYABLE_BUT_MOVABLE(ContainerMeshBuilders);

    ContainerMeshBuilders(const TileContainer& container, bool staticMeshIsOnlyQuads);
    void computeBoundingSpheres();

    void addMaterial(MaterialID id) { materialDependencies.emplace(id); }

    TileContainerID containerId;
    ContainerMeshDataCopy tileData;
    ProceduralMeshBuilder staticBuilder;
    ProceduralMeshBuilder dynamicBuilder;
    BillboardMeshBuilder billboardBuilder;
    InstancedStaticModelGatherer modelGatherer;
    World& world;
    std::unordered_set<MaterialID> materialDependencies;
};

