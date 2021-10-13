#pragma once
class CharacterDetailsComponent {
public:
    nString name;
};

struct CharacterDetailsComponentDef {
    const char* name;
};
KEG_TYPE_DECL(CharacterDetailsComponentDef);