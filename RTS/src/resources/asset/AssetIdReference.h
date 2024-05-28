#pragma once

class AssetIdReferenceBase {
public:
    bool isValid() const { return id != INVALID_ASSET_ID; }
    void invalidate() { id = INVALID_ASSET_ID; }

    AssetID id = INVALID_ASSET_ID;
protected:
    StrToken getAssetName(AssetType type);
    bool updateAndRenderImguiInternal(const char* label, AssetType type, AssetFilterFunc filterFunc);
};

// Super lightweight, successor to SoftAssetReference?
template <AssetType ASSET_TYPE>
class AssetIdReference : AssetIdReferenceBase {
public:
    inline static AssetType getAssetType() { return ASSET_TYPE; }

    // Asset select tool rendering
    bool updateAndRenderImgui(const char* label, AssetFilterFunc filterFunc = nullptr) {
        return updateAndRenderImguiInternal(label, ASSET_TYPE, filterFunc);
    }
};
