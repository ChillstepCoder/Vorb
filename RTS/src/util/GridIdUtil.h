#pragma once

// Utils for uniform square grid cells on a square map row major order,
// such as chunkIDs, blockIDs, ect
namespace GridIdUtil {
    // Does not bounds check
    inline ui32 getCellIndexFromWorldPos(f32v2 worldPos, ui32 cellWidth, ui32 widthCells) {
        const ui32 cellX = worldPos.x / cellWidth;
        const ui32 cellY = worldPos.y / cellWidth;
        return cellY * widthCells + cellX;
    }
    // Does not bounds check
    inline ui32 getCellIndexFromWorldPos(ui32v2 worldPos, ui32 cellWidth, ui32 widthCells) {
        const ui32 cellX = worldPos.x / cellWidth;
        const ui32 cellY = worldPos.y / cellWidth;
        return cellY * widthCells + cellX;
    }
    inline ui32v2 getCellXY(ui32 id, ui32 widthCells) {
        return ui32v2(id % widthCells, id / widthCells);
    }
    inline f32v2 getWorldPosCenter(ui32v2 cellXY, ui32 cellWidth) {
        return f32v2(cellXY * cellWidth) + f32v2(cellWidth * 0.5f);
    }
    inline f32v2 getWorldPosCenter(ui32 id, ui32 cellWidth, ui32 widthCells) {
        return getWorldPosCenter(getCellXY(id, widthCells), cellWidth);
    }
    inline ui32v2 getWorldPos(ui32v2 cellXY, ui32 cellWidth) {
        return cellXY * cellWidth;
    }
    inline ui32v2 getWorldPos(ui32 id, ui32 cellWidth, ui32 widthCells) {
        return getWorldPos(getCellXY(id, widthCells), cellWidth);
    }
}