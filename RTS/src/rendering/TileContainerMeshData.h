#pragma once

class Mesh;
class AssetHandleBundle;

struct TileContainerMeshData {
    TileContainerMeshData();
    ~TileContainerMeshData();

    TileContainerMeshData(TileContainerMeshData&& other) noexcept;
    TileContainerMeshData& operator=(TileContainerMeshData&& other) noexcept;

    VORB_NON_COPYABLE(TileContainerMeshData);

    std::unique_ptr<Mesh> mStaticMesh;
    std::unique_ptr<Mesh> mDynamicMesh;
    std::unique_ptr<Mesh> mBillboardMesh;

    std::unique_ptr<AssetHandleBundle> mAssetDependencies;

};