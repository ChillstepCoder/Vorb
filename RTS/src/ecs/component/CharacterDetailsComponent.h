#pragma once

#include "ecs/component/ComponentDefBase.h"

class CharacterDetailsComponent {
public:
    nString name;
};

class CharacterDetailsComponentDef : public ComponentDefBase {
public:
    nString name;
};
SERIALIZABLE_SIMPLE(CharacterDetailsComponentDef,
    make_field(o.name, "name"sv)
);