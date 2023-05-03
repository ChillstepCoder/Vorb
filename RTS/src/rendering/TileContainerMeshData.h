#pragma once

class Mesh;

struct TileContainerMeshData {
    TileContainerMeshData() = default;
    ~TileContainerMeshData();

    VORB_NON_COPYABLE_BUT_MOVABLE(TileContainerMeshData);

    std::unique_ptr<Mesh> mStaticMesh;
    std::unique_ptr<Mesh> mDynamicMesh;
    std::unique_ptr<Mesh> mBillboardMesh;
};