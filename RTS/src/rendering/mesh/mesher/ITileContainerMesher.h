#pragma once

class TileContainer;
class StaticPhysicsMeshBuilder;
class ContainerMeshBuilders;
class TileContainerRenderer;

class ITileContainerMesher {
protected:
    ITileContainerMesher(TileContainerRenderer& renderer) : mRenderer(renderer) {};
    void initMeshAndPhysicsAsyncInternal(const TileContainer& container, const f32* heightData, bool staticMeshIsOnlyQuads, ui32 reserveStaticVertexCount, const void* userData) const;

    virtual void addCustomMeshData(ContainerMeshBuilders& meshBuilders, StaticPhysicsMeshBuilder& physicsBuilder, const void* userData) const { UNUSED(meshBuilders); UNUSED(physicsBuilder); UNUSED(userData); }

    TileContainerRenderer& mRenderer;
};
