#pragma once

constexpr ui32 STEPS_PER_TILE = 4;
constexpr f32 STAIR_TILE_HEIGHT = 3.0f / 4.0f;
constexpr f32 stepHeight = STAIR_TILE_HEIGHT / STEPS_PER_TILE;

struct StairPiece {
    TileIndex pos;
    ui16 height;
    bool isFlatPart : 1;
    bool isLastPiece : 1;
    bool isBuilt : 1;
    bool isReserved : 1;
    Cartesian dir;
};