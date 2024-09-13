#pragma once

#include "rendering/mesh/MeshConst.h"
#include "rendering/model/MaterialRenderPassType.h"

constexpr ui32 MAX_MODEL_VARIANTS = 16;

struct ModelSubmeshData {
    StrToken name; // Run time only
    SubmeshID submeshId;
    MeshWindType windType;
    MaterialRenderPassType renderPass;

    bool castsShadow() const {
        return renderPass != MaterialRenderPassType::Water;
    }
};
SERIALIZABLE_IMGUI_CONTROLLED_DECL(ModelSubmeshData);

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
SERIALIZABLE_ENUM_DECL(ModelVariantSelectType);

SERIALIZABLE_DECL(std::array<MaterialAssetRef, MATERIAL_SLOT_COUNT>);

struct ModelVariantData {
    StrToken displayName = CStrToken("new_variant");
    std::vector<std::array<MaterialAssetRef, MATERIAL_SLOT_COUNT>> submeshMaterials;

    BINARY_SERIALIZE();
    BINARY_SERIALIZE_INPUT() {
        s.object(displayName);
        ui16 size;
        s.value2b(size);
        submeshMaterials.resize(size);
        for (auto&& arr : submeshMaterials) {
            for (ui32 i = 0; i < MATERIAL_SLOT_COUNT; ++i) {
                StrToken name;
                s.object(name);
                if (name.isValid()) {
                    arr[i] = MaterialAssetRef(name);
                }
            }
        }
    }
    BINARY_SERIALIZE_OUTPUT() {
        s.object(displayName);
        s.value2b((ui16)submeshMaterials.size());
        for (auto&& arr : submeshMaterials) {
            for (auto&& assetRef : arr) {
                if (assetRef.isValid()) {
                    s.object(assetRef.getAssetName());
                }
                else {
                    StrToken name;
                    s.object(name);
                }
            }
        }
    }
};
SERIALIZABLE_IMGUI_CONTROLLED(ModelVariantData,
    make_field(o.displayName, "name"sv),
    make_field(o.submeshMaterials, "submesh_mats"sv)
);