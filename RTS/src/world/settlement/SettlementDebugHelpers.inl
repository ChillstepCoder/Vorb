// Helper
inline bool isInfiniteTime(f32 time) {
    return time < 0.0f || time > 1.0f;
}

inline f32v3 helperGetWorldPosFromDTileCoord(DTileCoord pos, World* world) {
    if (pos.x < 0 || pos.y < 0 || pos.x > world->getWidthDTiles() || pos.y > world->getWidthDTiles()) [[unlikely]] {
        return f32v3(0.0f);
    }
    const f32 z = world->getHeightmapGrid().getHeightAtVert<true>(pos);
    const i32v2 tilePosA = pos.toTilePos();
    return f32v3(tilePosA.x, tilePosA.y, z);
};
inline f32v3 helperGetWorldPosFrom2DPos(f32v2 pos, World* world) {
    if (pos.x < 0 || pos.y < 0 || pos.x > world->getWidthTiles() || pos.y > world->getWidthTiles()) [[unlikely]] {
        return f32v3(0.0f);
    }
    const f32 z = world->getHeightmapGrid().computeHeightAtPoint<true>(pos);
    return f32v3(pos.x, pos.y, z);
};

inline void helperAddVisLogLineBetweenCoords(VisualLog* log, DTileCoord a, DTileCoord b, World* world, color4 color) {
    if (log) {
        log->addLineBetweenPoints(helperGetWorldPosFromDTileCoord(a, world), helperGetWorldPosFromDTileCoord(b, world), color);
    }
}

inline void helperAddVisLogFilledQuadAtCoord(VisualLog* log, DTileCoord p, f32v2 size, World* world, color4 color) {
    if (log) {
        const f32v3 pos = helperGetWorldPosFromDTileCoord(p, world);
        const f32v3 s3d(size.x, size.y, 0.0f);
        log->addFilledQuad(pos - s3d * 0.5f, size, color);
    }
}

inline void helperAddVisLogLineBetweenPos(VisualLog* log, f32v2 a, f32v2 b, World* world, color4 color) {
    if (log) {
        log->addLineBetweenPoints(helperGetWorldPosFrom2DPos(a, world), helperGetWorldPosFrom2DPos(b, world), color);
    }
}

inline void helperAddVisLogFilledQuadAtPos(VisualLog* log, f32v2 p, f32v2 size, World* world, color4 color) {
    if (log) {
        const f32v3 pos = helperGetWorldPosFrom2DPos(p, world);
        const f32v3 s3d(size.x, size.y, 0.0f);
        log->addFilledQuad(pos - s3d * 0.5f, size, color);
    }
}

inline void helperAddTextAtPos(VisualLog* log, f32v2 p, const std::string& text, World* world, color4 color) {
    if (log) {
        const f32v3 pos = helperGetWorldPosFrom2DPos(p, world);
        log->addText(text, pos, 2.0f, f32v2(0.0f, 2.5f), color);
    }
}