#pragma once

class HeightmapTerrainQuadtree;
class WorldGrid;

class TerrainMeshManager
{
public:
    TerrainMeshManager(WorldGrid& worldGrid);
    ~TerrainMeshManager();

    void update(const f32v2& loadCenter, bool forceUpdate);

    // TODO: Call this
    void dirtyTerrainFromBrush(const f32v2& pos, f32 brushRadius);
    void dirtyAllTerrain();

    const std::vector<HeightmapTerrainQuadtree>& getTerrainQuadtrees() const { return mTerrainTrees; }

private:
    TickingTimer mUpdateTimer = TickingTimer(60.0);
    std::vector<HeightmapTerrainQuadtree> mTerrainTrees;
};

