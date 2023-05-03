#pragma once

#include <boost/container/flat_set.hpp>

class MaterialShader;
class GrassMesh;
class Camera3D;

class GrassRenderer
{
public:
    GrassRenderer();
    void renderGrass(const Camera3D& camera, const f32v3& playerPos, const boost::container::flat_set<const GrassMesh*>& grassMeshes);

private:
    const MaterialShader* mGrassMaterial = nullptr;
};

