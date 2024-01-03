#pragma once

#include "rendering/mesh/MeshConst.h"

constexpr ui32 MAX_MODEL_VARIANTS = 8;

struct ModelSubmeshData {
    MeshWindType windType;
};
SERIALIZABLE_IMGUI_CONTROLLED(ModelSubmeshData,
    make_field(o.windType, "wind"sv)
);

struct ModelVariantGpuData {
    // TODO: We could bitpack 4 bytes into one of these?
    ui32 material;
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
    std::vector<SoftAssetReference> submeshMaterials; 
    f32 weight = 1.0f;
};
SERIALIZABLE_IMGUI_CONTROLLED(ModelVariantData,
    make_field(o.submeshMaterials, "submesh_mats"sv),
    make_field(o.weight, "weight"sv)
);