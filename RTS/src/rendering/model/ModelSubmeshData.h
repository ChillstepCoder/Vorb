#pragma once

#include "rendering/mesh/MeshConst.h"

constexpr ui32 MAX_MODEL_VARIANTS = 16;

struct ModelSubmeshData {
    StrToken name; // Run time only
    MeshWindType windType;
};
SERIALIZABLE_IMGUI_CONTROLLED(ModelSubmeshData,
    make_field(o.windType, "wind"sv)
);

// Matches std140 layout
constexpr ui32 MATERIAL_SLOT_COUNT = 4;
struct alignas(ui32v4) ModelVariantGpuData {
    ui32v4 materials; // 4 material slots per model. 3 Base, 1 damage
    static_assert(MATERIAL_SLOT_COUNT == 4);
};

using ModelVariantGpuDataContainer = std::vector<ModelVariantGpuData>;

enum class ModelVariantSelectType : ui8 {
    Random,
    Voronoi
};
SERIALIZABLE_ENUM_SAME_NAME(ModelVariantSelectType,
    ENUM_FIELD_SIMPLE(ModelVariantSelectType, Random),
    ENUM_FIELD_SIMPLE(ModelVariantSelectType, Voronoi)
);

YML_WRITE_DEF(std::array<MaterialAssetRef, MATERIAL_SLOT_COUNT>) {
    ryml::NodeRef& nr = *n;
    nr |= ryml::SEQ;
    nr |= ryml::_WIP_STYLE_FLOW_SL;
    for (int i = 0; i < MATERIAL_SLOT_COUNT; ++i) {
        nr.append_child() << o[i];
    }
}
YML_READ_DEF(std::array<MaterialAssetRef, MATERIAL_SLOT_COUNT>) {
    if (n.num_children() > MATERIAL_SLOT_COUNT) return false;
    int i = 0;
    for (auto const ch : n)
        ch >> (*target)[i++];
    return true;
}

struct ModelVariantData {
    StrToken displayName = CStrToken("new_variant");
    std::vector<std::array<MaterialAssetRef, MATERIAL_SLOT_COUNT>> submeshMaterials;
};
SERIALIZABLE_IMGUI_CONTROLLED(ModelVariantData,
    make_field(o.displayName, "name"sv),
    make_field(o.submeshMaterials, "submesh_mats"sv)
);