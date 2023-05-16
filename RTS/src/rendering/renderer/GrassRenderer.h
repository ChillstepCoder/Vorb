#pragma once

#include <boost/container/flat_set.hpp>

#include "rendering/mesh/TileGrassMeshType.h"

class MaterialShader;
class GrassMesh;
class Camera3D;

class GrassRenderer
{
public:
    GrassRenderer();
    void renderGrass(const Camera3D& camera, const f32v3& playerPos, const boost::container::flat_set<const GrassMesh*>& grassMeshes, TileGrassMeshType meshType);

private:
    void renderDefaultGrass(const Camera3D& camera, const f32v3& playerPos, const boost::container::flat_set<const GrassMesh*>& grassMeshes);
    void renderPlaneGrass(const Camera3D& camera, const f32v3& playerPos, const boost::container::flat_set<const GrassMesh*>& grassMeshes);
    const MaterialShader* mGrassMaterial = nullptr;
};

