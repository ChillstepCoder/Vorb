#pragma once

enum class MeshIndexType : ui16 {
    INVALID = 0,
    USHORT = GL_UNSIGNED_SHORT,
    UINT = GL_UNSIGNED_INT
};

namespace util {
    inline size_t getMeshIndexSizeBytes(MeshIndexType type) {
        switch (type) {
            case MeshIndexType::USHORT:
                return sizeof(ui16);
            case MeshIndexType::UINT:
                return sizeof(ui32);
            default:
                assert(false);
                return 0;
        }
    }
}