#pragma once

#include <ozz/animation/runtime/animation.h>

class AnimationDef : public IAsset {
public:
    DEFAULT_ASSET_CONSTRUCTOR(AnimationDef);

    ozz::animation::Animation mAnimation;
};
