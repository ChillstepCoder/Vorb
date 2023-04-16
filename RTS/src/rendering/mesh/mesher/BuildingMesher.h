#pragma once

#include "city/Building.h"
#include "rendering/mesh/mesher/ITileContainerMesher.h"

class VisualLog;
class InstancedStaticModelGatherer;
class ResourceManager;
class Building;
class ProceduralMeshBuilder;
class BillboardMeshBuilder;
class PhysicsWorld;
struct MaterialData;

struct RoofContourEdgeInfo {
    f32v3 v1;
    f32v3 parent1;
    f32v3 v2;
    f32v3 parent2;
    Cartesian dir;
    bool isGable;
};


class BuildingMesher : public ITileContainerMesher
{
public:
    BuildingMesher(TileContainerRenderer& renderer) : ITileContainerMesher(renderer) {}

    void buildMeshAndPhysicsAsync(const Building& building) const;

private:
    void addCustomMeshData(ContainerMeshBuilders& meshBuilders, StaticPhysicsMeshBuilder& physicsBuilder, const void* userData) const;
 
};
