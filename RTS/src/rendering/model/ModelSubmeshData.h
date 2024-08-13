#pragma once

#include "rendering/mesh/MeshConst.h"
#include "serialization/BitseryExt.h"

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

    BINARY_SERIALIZE();
    BINARY_SERIALIZE_INPUT() {
        s.ext(displayName, bitsery::ext::PodStruct{});
        ui16 size;
        s.value2b(size);
        submeshMaterials.resize(size);
        for (auto&& arr : submeshMaterials) {
            for (ui32 i = 0; i < MATERIAL_SLOT_COUNT; ++i) {
                StrToken name;
                s.ext(name, bitsery::ext::PodStruct{});
                if (name.isValid()) {
                    arr[i] = MaterialAssetRef(name);
                }
            }
        }
    }
    BINARY_SERIALIZE_OUTPUT() {
        s.ext(displayName, bitsery::ext::PodStruct{});
        s.value2b((ui16)submeshMaterials.size());
        for (auto&& arr : submeshMaterials) {
            for (auto&& assetRef : arr) {
                if (assetRef.isValid()) {
                    s.ext(assetRef.getAssetName(), bitsery::ext::PodStruct{});
                }
                else {
                    s.ext(StrToken(), bitsery::ext::PodStruct{});
                }
            }
        }
    }
};
SERIALIZABLE_IMGUI_CONTROLLED(ModelVariantData,
    make_field(o.displayName, "name"sv),
    make_field(o.submeshMaterials, "submesh_mats"sv)
);