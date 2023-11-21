#pragma once


class BiomeDef : public IAsset {
public:
    DEFAULT_ASSET_CONSTRUCTOR(BiomeDef);

    f32 priority = 0.0f;
};
SERIALIZABLE_SIMPLE(BiomeDef,
    make_field(o.priority, "priority"sv)
);