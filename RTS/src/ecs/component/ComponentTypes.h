#pragma once

// All supported component types a
enum class ComponentTypes : ui8 {
    CharacterModel,
    CharacterControl,
    Combat,
    Corpse,
    CharacterDetails,
    DynamicLight,
    Inventory,
    Navigation,
    PersonAI,
    Physics,
    Profession,
    Skills,
    COUNT
    // TODO: Custom
};
constexpr StrToken ComponentTypeStrings[e_cast(ComponentTypes::COUNT)] = {
    CStrToken("character_model"),
    CStrToken("character"),
    CStrToken("combat"),
    CStrToken("corpse"),
    CStrToken("character_details"),
    CStrToken("dynamic_light"),
    CStrToken("inventory"),
    CStrToken("navigation"),
    CStrToken("person_ai"),
    CStrToken("physics"),
    CStrToken("profession"),
    CStrToken("skills")
};
static_assert(e_cast(ComponentTypes::COUNT) == 12, "Update .ent file type strings");