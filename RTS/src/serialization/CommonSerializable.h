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


// ASSET TYPE
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