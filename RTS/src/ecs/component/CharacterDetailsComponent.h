#pragma once
class CharacterDetailsComponent {
public:
    nString name;
};

struct CharacterDetailsComponentDef {
    const char* name = nullptr;
};
KEG_TYPE_DECL(CharacterDetailsComponentDef);