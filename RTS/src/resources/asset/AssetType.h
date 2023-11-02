#pragma once

enum class AssetType : ui8 {
    Tile,
    ParticleSystem,
    Effect,
    Texture,
    Cubemap,
    Brush,
    Material,
    Rig,
    Animation,
    AnimMachine,
    Model,
    Skill,
    Item,
    Fish,
    MaterialShader,
    TileGrass,
    NONE,
    COUNT = NONE
};