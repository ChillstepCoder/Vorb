#pragma once

class TileContainer;
class StaticPhysicsMeshBuilder;
class ContainerMeshBuilders;
class TileContainerMeshManager;

class ITileContainerMesher {
protected:
    ITileContainerMesher(TileContainerMeshManager& meshManager) : mMeshManager(meshManager) {};
    void initMeshAndPhysicsAsyncInternal(const TileContainer& container, const f32* heightData, bool staticMeshIsOnlyQuads, ui32 reserveStaticVertexCount, const void* userData) const;

    virtual void addCustomMeshData(ContainerMeshBuilders& meshBuilders, StaticPhysicsMeshBuilder& physicsBuilder, const void* userData) const { UNUSED(meshBuilders); UNUSED(physicsBuilder); UNUSED(userData); }

    TileContainerMeshManager& mMeshManager;
};
