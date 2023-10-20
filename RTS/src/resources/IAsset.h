#pragma once

// TODO: Stdafx?
#include "serialization/YmlSerializer.h"

class IAssetRepositoryBase;

#define DEFAULT_ASSET_CONSTRUCTOR(Type) \
    Type(StrToken name, AssetID id) : IAsset(name, id) {};

enum class AssetType : ui8 {
    Tile,
    ParticleSystem,
    Effect,
    Texture,
    Cubemap,
    Brush,
    Material,
    Rig,
    Animation,
    AnimMachine,
    Model,
    Skill,
    Item,
    Fish,
    MaterialShader,
    TileGrass,
    NONE,
    COUNT = NONE
};
SERIALIZABLE_ENUM_SAME_NAME(AssetType,
    pair{ AssetType::Tile, "tile"sv },
    pair{ AssetType::ParticleSystem, "particle_system"sv },
    pair{ AssetType::Effect, "effect"sv },
    pair{ AssetType::Texture, "texture"sv },
    pair{ AssetType::Cubemap, "cubemap"sv },
    pair{ AssetType::Brush, "brush"sv },
    pair{ AssetType::Material, "material"sv },
    pair{ AssetType::Rig, "rig"sv },
    pair{ AssetType::Animation, "animation"sv },
    pair{ AssetType::AnimMachine, "anim_machine"sv },
    pair{ AssetType::Model, "model"sv },
    pair{ AssetType::Skill, "skill"sv },
    pair{ AssetType::Item, "item"sv },
    pair{ AssetType::Fish, "fish"sv },
    pair{ AssetType::MaterialShader, "material_shader"sv },
    pair{ AssetType::TileGrass, "tile_grass"sv },
)
static_assert(e_count(AssetType) == 16);

class AssetHandleBundle;
class AssetHandleBase;

class IAsset {
public:
    IAsset(StrToken name, AssetID id);
    virtual ~IAsset();

    VORB_MOVABLE(IAsset);

    StrToken getName() const { return mName; }
    void setName(StrToken name) { mName = name; }

    AssetID getID() const { return mID; }

    bool isDirty() const { return mDirty; }
    void setDirty(bool val) const { mDirty = val; }
    AssetHandleBundle* getDependencies() const { return mDependencies.get(); }
    void addDependency(std::unique_ptr<AssetHandleBase>&& handle);
    void reserveDependencyCount(size_t count);

protected:
    std::unique_ptr<AssetHandleBundle> mDependencies;
    StrToken mName;
    AssetID mID = INVALID_ASSET_ID;
    mutable bool mDirty = false;
};

template <typename T>
concept IsAssetType = std::is_base_of<IAsset, T>::value;

// Represents a unique ID for an asset which can be used to look it up
struct AssetDescriptor {

    bool isValid() const { return assetType != AssetType::NONE; }
    
    bool operator<(const AssetDescriptor& other) const {
        if (id != other.id) return id < other.id;
        return assetType < other.assetType;
    }

    static AssetDescriptor fromUUID(UniqueId64 uid) {
        AssetDescriptor descriptor;
        descriptor.assetType = static_cast<AssetType>(uid >> 32ull);
        // Indicates probably not a valid UUID
        if (descriptor.assetType >= AssetType::NONE) {
            LOG_WARN("Tried to convert an invalid asset UID {} to an AssetDescriptor", static_cast<ui64>(uid));
            descriptor.assetType = AssetType::NONE;
            return descriptor;
        }
        descriptor.id = uid & 0xffffffffull;
        return descriptor;
    }

    UniqueId64 getUUID() const {
        return UniqueId64((ui64)id | ((ui64)assetType << 32ull));
    }
    StrToken getName() const;
    std::filesystem::path getPath() const;
    IAssetRepositoryBase* getRepo() const;

    // Data
    AssetID id = INVALID_ASSET_ID;
    AssetType assetType = AssetType::NONE;
};