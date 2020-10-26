#pragma once
#include <Vorb/ecs/ComponentTable.hpp>
#include "rendering/LightData.h"

struct DynamicLightComponent {
    LightData mLightData;
    vecs::ComponentID mPhysicsComponent = 0;
};

class DynamicLightComponentTable : public vecs::ComponentTable<DynamicLightComponent> {
public:
    static const std::string& NAME;
};

