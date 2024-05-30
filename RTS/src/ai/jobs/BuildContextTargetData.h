#pragma once

struct BuildContextTargetData {
    i32 targetIndex = std::numeric_limits<i32>::max();
    enum class Type : i8 {
        Tile,
        Wall,
        Stairs,
        COUNT
    } type;

    bool isValid() const {
        return targetIndex != std::numeric_limits<i32>::max();
    }
    void invalidate() {
        targetIndex = std::numeric_limits<i32>::max();
    }
};