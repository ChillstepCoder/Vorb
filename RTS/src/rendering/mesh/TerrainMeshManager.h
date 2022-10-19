#pragma once

class HeightmapTerrainQuadtree;
class TerrainMeshManager
{
public:
    TerrainMeshManager();
    ~TerrainMeshManager();

    void tick();

    // TODO: Call this
    void dirtyTerrainFromBrush(const f32v2& pos, f32 brushRadius);
    void dirtyAllTerrain();

    const std::vector<HeightmapTerrainQuadtree>& getTerrainQuadtrees() const { return mTerrainTrees; }

private:
    std::vector<HeightmapTerrainQuadtree> mTerrainTrees;
};

