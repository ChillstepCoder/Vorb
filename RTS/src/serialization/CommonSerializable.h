#pragma once

// STRTOKEN
YML_WRITE_DEF(StrToken) {
    ryml::NodeRef& nr = *n;
    nr << o.toString();
}
YML_READ_DEF(StrToken) {
    c4::csubstr str;
    n >> str;
    if (str.size() > MAX_CHARS_IN_STRTOKEN) {
        panic("Invalid string token length (max 20) {} {}", str.size(), str.data());
    };
    *target = StrToken(str.data(), str.size());
    return true;
}

// SOFT ASSET REFERENCE
YML_WRITE_DEF(SoftAssetReference) {
    ryml::NodeRef& nr = *n;
    nr << o.name.toString();
}
YML_READ_DEF(SoftAssetReference) {
    c4::csubstr str;
    n >> str;
    if (str.size() > MAX_CHARS_IN_STRTOKEN) {
        panic("Invalid asset reference strtoken length (max 20) {} {}", str.size(), str.data());
    };
    target->name = StrToken(str.data(), str.size());
    return true;
}

SERIALIZABLE_ENUM_SAME_NAME(AssetType,
    ENUM_FIELD_SIMPLE(AssetType, Tile),
    ENUM_FIELD_SIMPLE(AssetType, ParticleSystem),
    ENUM_FIELD_SIMPLE(AssetType, Effect),
    ENUM_FIELD_SIMPLE(AssetType, Texture),
    ENUM_FIELD_SIMPLE(AssetType, Cubemap),
    ENUM_FIELD_SIMPLE(AssetType, Brush),
    ENUM_FIELD_SIMPLE(AssetType, Material),
    ENUM_FIELD_SIMPLE(AssetType, Rig),
    ENUM_FIELD_SIMPLE(AssetType, Animation),
    ENUM_FIELD_SIMPLE(AssetType, AnimMachine),
    ENUM_FIELD_SIMPLE(AssetType, Model),
    ENUM_FIELD_SIMPLE(AssetType, Skill),
    ENUM_FIELD_SIMPLE(AssetType, Item),
    ENUM_FIELD_SIMPLE(AssetType, Fish),
    ENUM_FIELD_SIMPLE(AssetType, MaterialShader),
    ENUM_FIELD_SIMPLE(AssetType, TileGrass),
    ENUM_FIELD_SIMPLE(AssetType, Biome),
    ENUM_FIELD_SIMPLE(AssetType, TileDistribution)
);
static_assert(e_count(AssetType) == 18);

// Usage: s.value2b(myValue) ect...
// See bitsery documentation
// Put this at the BOTTOM of the class definition
#define BINARY_SERIALIZE() \
private: \
  friend bitsery::Access; \
  template <typename S>  \
  void serialize(S& s)

// Must have BINARY_SERIALIZE(); defined first.
#define BINARY_SERIALIZE_INPUT() \
  template <> \
  void serialize<bitsery::Deserializer<BInputAdapter>>(bitsery::Deserializer<BInputAdapter>& s)

// Must have BINARY_SERIALIZE(); defined first.
#define BINARY_SERIALIZE_OUTPUT() \
  template <> \
  void serialize<bitsery::Serializer<BOutputAdapter>>(bitsery::Serializer<BOutputAdapter>& s)