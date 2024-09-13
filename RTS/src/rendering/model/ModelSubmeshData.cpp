#include "stdafx.h"
#include "ModelSubmeshData.h"


SERIALIZABLE_IMGUI_CONTROLLED(ModelSubmeshData,
    make_field(o.windType, "wind"sv)
);

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