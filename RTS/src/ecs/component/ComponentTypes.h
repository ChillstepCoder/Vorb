#pragma once

// All supported component types a
enum class ComponentTypes {
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
    SimpleSprite,
    SoldierAI,
    UndeadAI,
    Skills,
    COUNT
    // TODO: Custom
};
const nString ComponentTypeStrings[e_cast(ComponentTypes::COUNT)] = {
    "character_model",
    "character",
    "combat",
    "corpse",
    "character_details",
    "dynamic_light",
    "inventory",
    "navigation",
    "person_ai",
    "physics",
    "profession",
    "simple_sprite",
    "soldier_ai",
    "undead_ai",
    "skills"
};
static_assert(e_cast(ComponentTypes::COUNT) == 15, "Update .ent file type strings");