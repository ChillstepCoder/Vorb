#pragma once

#include "rendering/mesh/MeshConst.h"

constexpr ui32 MAX_MODEL_VARIANTS = 16;

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
    StrToken displayName = CStrToken("new_variant");
    std::vector<SoftAssetReference> submeshMaterials;
};
SERIALIZABLE_IMGUI_CONTROLLED(ModelVariantData,
    make_field(o.displayName, "name"sv),
    make_field(o.submeshMaterials, "submesh_mats"sv)
);