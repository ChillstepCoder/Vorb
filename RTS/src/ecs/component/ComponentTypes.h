#pragma once

// All supported component types a
enum class ComponentTypes {
    CharacterModel,
    Combat,
    Corpse,
    DynamicLight,
    Navigation,
    PersonAI,
    Physics,
    PlayerControl,
    Profession,
    SimpleSprite,
    SoldierAI,
    UndeadAI,
    COUNT
    // TODO: Custom
};
const nString ComponentTypeStrings[enum_cast(ComponentTypes::COUNT)] = {
    "character_model",
    "combat",
    "corpse",
    "dynamic_light",
    "navigation",
    "person_ai",
    "physics",
    "player_control",
    "profession",
    "simple_sprite",
    "soldier_ai",
    "undead_ai"
};
static_assert(enum_cast(ComponentTypes::COUNT) == 12, "Update .ent file type strings");