#pragma once

// TODO: Could this become TileInteractable? ResourceTag?
enum class TileHarvestable : ui8 {
    Wood,
    WoodBirch,
    Stone,
    None,
    COUNT = None
};
SERIALIZABLE_ENUM_DECL(TileHarvestable);