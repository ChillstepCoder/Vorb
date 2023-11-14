#pragma once

#include "terrain/CompressedHeight.h"

class TileContainer;
class StaticPhysicsMeshBuilder;
class ContainerMeshBuilders;
class TileContainerMeshManager;

class ITileContainerMesher {
protected:
    ITileContainerMesher(TileContainerMeshManager& meshManager) : mMeshManager(meshManager) {};
    void initMeshAndPhysicsAsyncInternal(const TileContainer& container, const CompressedHeight* heightData, bool staticMeshIsOnlyQuads, ui32 reserveStaticVertexCount, const void* userData) const;

    virtual void addCustomMeshData(ContainerMeshBuilders& meshBuilders, StaticPhysicsMeshBuilder& physicsBuilder, const void* userData) const { UNUSED(meshBuilders); UNUSED(physicsBuilder); UNUSED(userData); }

    TileContainerMeshManager& mMeshManager;
};
