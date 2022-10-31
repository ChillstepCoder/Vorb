#pragma once

class Material;
class ChunkGrassQuadtree;
class Camera3D;

class GrassRenderer
{
public:
    GrassRenderer();
    void renderGrass(const std::set<const ChunkGrassQuadtree*>& grassQuadtrees, const Camera3D& camera, const f32v3& playerPos);

private:
    const Material* mGrassMaterial = nullptr;
};

