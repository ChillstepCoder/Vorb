#pragma once

#include "resources/IAsset.h"

// Non template base so we can define functions in .cpp and not need to include ResourceManager.h
class LiteAssetRefBase {
public:
    bool isValid() const { return mId != INVALID_ASSET_ID; }
    void invalidate() { mId = INVALID_ASSET_ID; }
    AssetID getAssetID() const { return mId; }
    void setAssetID(AssetID id) { mId = id; }

    std::size_t hash() const {
        return boost::hash<AssetID>{}(mId);
    }

protected:

    bool isLoadedInternal(AssetType type) const;
    StrToken getAssetNameInternal(AssetType type) const;
    void setAssetNameInternal(StrToken name, AssetType type);
    bool updateAndRenderImguiInternal(const char* label, AssetType type, AssetFilterFunc filterFunc);
    IAsset& getLoadedOrUnloadedAssetInternal(AssetType type) const;
    IAsset& getLoadedAssetInternal(AssetType type) const;
    AssetHandleBasePtr getAssetHandleBaseInternal(AssetType type) const;

    AssetID mId = INVALID_ASSET_ID;
};

// Super lightweight 4 byte asset ref
template <AssetType assetType>
class LiteAssetRef : public LiteAssetRefBase {
public:
    LiteAssetRef() = default;
    LiteAssetRef(AssetID id) { mId = id; }
    LiteAssetRef(StrToken name) { setAssetName(name); }

    inline static constexpr AssetType getAssetType() { return assetType; }

    StrToken getAssetName() const { return getAssetNameInternal(assetType); }
    void setAssetName(StrToken name) { setAssetNameInternal(name, assetType); }

    void toString(OUT char* outStr, OUT ui32* outLength) const { getAssetNameInternal(assetType).toString(outStr, outLength); }
    nString toString() const { return getAssetNameInternal(assetType).toString(); }

    AssetDescriptor getAssetDescriptor() const { return AssetDescriptor{ .id = mId, .assetType = assetType, }; }
    AssetHandleBasePtr getAssetHandleBase() const { return getAssetHandleBaseInternal(assetType); }
    
    bool isLoaded() const {
        return isLoadedInternal(assetType);
    }

    template<typename AssetClass>
    AssetHandlePtr<AssetClass> getAssetHandle() const {
        assert(AssetClass::ASSET_TYPE == assetType);
        return static_unique_pointer_cast<AssetHandle<AssetClass>>(getAssetHandleBase());
    }

    template <typename AssetClass>
    const AssetClass& getLoadedAsset() const {
        assert(AssetClass::ASSET_TYPE == assetType);
        return static_cast<AssetClass&>(getLoadedAssetInternal(assetType));
    }

    template <typename AssetClass>
    const AssetClass& getLoadedOrUnloadedAsset() const {
        assert(AssetClass::ASSET_TYPE == assetType);
        return static_cast<AssetClass&>(getLoadedOrUnloadedAssetInternal(assetType));
    }

    template <typename AssetClass>
    AssetClass& getMutableLoadedOrUnloadedAsset() const {
        assert(AssetClass::ASSET_TYPE == assetType);
        return static_cast<AssetClass&>(getLoadedOrUnloadedAssetInternal(assetType));
    }

    // Asset select tool rendering
    bool updateAndRenderImgui(const char* label, AssetFilterFunc filterFunc = nullptr) {
        return updateAndRenderImguiInternal(label, assetType, filterFunc);
    }

    bool operator==(const LiteAssetRef& other) const {
        return mId == other.mId;
    }
    bool operator<(const LiteAssetRef& other) const {
        return mId < other.mId;
    }

    auto operator<=>(const LiteAssetRef&) const = default;
};

template <typename T>
concept IsLiteAssetRef = std::derived_from<T, LiteAssetRefBase>&& requires {
    { T::getAssetType() } -> std::convertible_to<AssetType>;
};


// Usage: ASSET_REF_DECL(Texture) yields TextureRef from TextureDef
#define ASSET_REF_DECL(Name) \
    using Name##AssetRef = LiteAssetRef<AssetType::Name>;

ASSET_REF_DECL(Tile);
ASSET_REF_DECL(ParticleSystem);
ASSET_REF_DECL(Effect);
ASSET_REF_DECL(Entity);
ASSET_REF_DECL(Texture);
ASSET_REF_DECL(Cubemap);
ASSET_REF_DECL(Brush);
ASSET_REF_DECL(Material);
ASSET_REF_DECL(Rig);
ASSET_REF_DECL(Animation);
ASSET_REF_DECL(AnimMachine);
ASSET_REF_DECL(Blendspace1D);
ASSET_REF_DECL(Model);
ASSET_REF_DECL(Skill);
ASSET_REF_DECL(Item);
ASSET_REF_DECL(Fish);
ASSET_REF_DECL(MaterialShader);
ASSET_REF_DECL(TileGrass);
ASSET_REF_DECL(Biome);
ASSET_REF_DECL(TileDistribution);
ASSET_REF_DECL(Building);
ASSET_REF_DECL(Room);
static_assert(e_count(AssetType) == 22, "Add AssetRef");

template <AssetType assetType>
inline size_t hash_value(const LiteAssetRef<assetType>& ref) {
    return ref.hash();
};
