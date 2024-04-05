#pragma once

#include "building/building.h"
#include "rendering/mesh/mesher/ITileContainerMesher.h"

class Building;

struct RoofContourEdgeInfo {
    f32v3 v1;
    f32v3 parent1;
    f32v3 v2;
    f32v3 parent2;
    f32v3 gablePos;
    Cartesian dir;
    bool isGable;
};

class BuildingMesher : public ITileContainerMesher
{
public:
    BuildingMesher(TileContainerMeshManager& meshManager) : ITileContainerMesher(meshManager) {}

    void buildMeshAndPhysicsAsync(const Building& building) const;

private:
    void addCustomMeshData(ContainerMeshBuilders& meshBuilders, StaticPhysicsMeshBuilder& physicsBuilder, const void* userData) const;
 
};
