#pragma once

inline i32 computeMipmapCount(const ui32v2& dims, i32 mipmapLevels) {
    // Determine The Maximum Number Of Mipmap Levels Available
    i32 maxMipmapLevels = 0;
    i32 size = (i32)glm::min(dims.x, dims.y);
    while (size > 1) {
        maxMipmapLevels++;
        size >>= 1;
    }

    // Get the number of mipmaps for this image
    mipmapLevels = MIN(mipmapLevels, maxMipmapLevels);
    return mipmapLevels;
}
