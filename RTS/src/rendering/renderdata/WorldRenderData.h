#pragma once

#include <boost/container/flat_set.hpp>

class GrassMesh;
class TerrainMesh;
class Mesh;

struct WorldRenderData {
    std::set<const GrassMesh*> mGrassMeshes;
    std::set<const TerrainMesh*> mTerrainMeshes;
    std::set<const TerrainMesh*> mTerrainWaterMeshes;

    boost::container::flat_set<const Mesh*> mStaticMeshes;
    boost::container::flat_set<const Mesh*> mDynamicMeshes;
    boost::container::flat_set<const Mesh*> mBillboardMeshes;
};
