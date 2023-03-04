#pragma once

// TODO: Could this become TileInteractable?
enum class TileHarvestable : ui8 {
    WOOD,
    STONE,
    NONE,
    COUNT = NONE
};
KEG_ENUM_DECL(TileHarvestable);