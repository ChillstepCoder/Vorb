#pragma once
class CharacterDetailsComponent {
public:
    nString name;
};

struct CharacterDetailsComponentDef {
    nString name;
};
SERIALIZABLE_SIMPLE(CharacterDetailsComponentDef,
    make_field(o.name, "name"sv)
);