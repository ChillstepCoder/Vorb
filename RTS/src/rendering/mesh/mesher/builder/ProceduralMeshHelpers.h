#pragma once

class ProceduralMeshBuilder;
class StaticPhysicsMeshBuilder;

namespace ProceduralMeshHelpers
{
    //void addWindowMesh(ui32v3 tileDims, f32 floorHeight, ProceduralMeshBuilder& meshBuilder, StaticPhysicsMeshBuilder& physMesh);
    void addTileWallMesh(Cartesian dir, const f32v3& tilePos, f32 wallHeight, ProceduralMeshBuilder& meshBuilder, StaticPhysicsMeshBuilder& physMesh, bool adjNegative, bool adjPositive);
};

