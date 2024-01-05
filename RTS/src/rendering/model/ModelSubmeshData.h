#pragma once

#include "rendering/mesh/MeshConst.h"

constexpr ui32 MAX_MODEL_VARIANTS = 16;

struct ModelSubmeshData {
    MeshWindType windType;
};
SERIALIZABLE_IMGUI_CONTROLLED(ModelSubmeshData,
    make_field(o.windType, "wind"sv)
);

// Matches std140 layout
struct alignas(ui32v4) ModelVariantGpuData {
    ui32 material;
    ui32 padding[3];
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

struct ModelVariantData {
    StrToken displayName = CStrToken("new_variant");
    std::vector<SoftAssetReference> submeshMaterials;
};
SERIALIZABLE_IMGUI_CONTROLLED(ModelVariantData,
    make_field(o.displayName, "name"sv),
    make_field(o.submeshMaterials, "submesh_mats"sv)
);