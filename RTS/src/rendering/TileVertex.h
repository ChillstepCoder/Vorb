#pragma once

enum class ShadowState : ui8 {
    NONE,
    THIN, // Thin objects like trees
    LEFT, // Top left vertex on thick objects
    RIGHT //  Top right vertex on thick objects
};

struct TileVertex {
public:
    TileVertex() {};
    TileVertex(const f32v3& pos, const f32v2& uvs, const color4& color, ui16 atlasPage) :
        pos(pos), uvs(uvs), color(color), atlasPage(atlasPage) {
    }

    f32v3 pos; // TOOD: ui16v3?
    f32v2 uvs; //TODO: ui16v2?
    color4 color;
    ui16 atlasPage;
    i8v3 normal; // TODO: 3D test only
    ui8 windInfluence = 0;
    ui8 padding[2];
};

// Need power of 2 alignment
static_assert(sizeof(TileVertex) == 32, "Power of 2 byte alignment needed");

struct BillboardVertex {
public:
    BillboardVertex() {};
    BillboardVertex(const f32v3& pos, const f32v2& xzOffset, const f32v2& uvs, const color4& color, ui16 atlasPage) :
        rootPos(pos), xzOffset(xzOffset), uvs(uvs), color(color), atlasPage(atlasPage) {
    }

    f32v3 rootPos;
    f32v2 xzOffset; // TOOD: ui16v2?
    f32v2 uvs; //TODO: ui16v2?
    color4 color;
    ui16 atlasPage;
    ui8 windInfluence = 0;
    ui8 PADDING_NEED_TO_COMPRESS[29];
};
// Need power of 2 alignment
// 64 is bad!!!
static_assert(sizeof(BillboardVertex) == 64, "Power of 2 byte alignment needed");