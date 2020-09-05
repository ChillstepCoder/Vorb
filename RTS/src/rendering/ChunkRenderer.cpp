#include "stdafx.h"
#include "ChunkRenderer.h"
#include "TileSet.h"
#include "Camera2D.h"
#include "world/Chunk.h"
#include "ResourceManager.h"

#include <Vorb/graphics/SpriteBatch.h>
#include <Vorb/graphics/SamplerState.h>
#include <Vorb/graphics/DepthState.h>


// TODO: Method(s) file?
struct ConnectedWallData {
	union {
		struct {
			ui8 a;
			ui8 b;
			ui8 c;
			ui8 d;
        };
        ui8 dataArray[4];
		ui32 data;
	};
};

ConnectedWallData sConnectedWallData[256];
const ui16 sExposedWallLookup[4] = { 0x13, 0x12, 0x14, 0x15 };

enum NeighborBits {
	BOTTOM_LEFT =  1 << 0,
	BOTTOM =       1 << 1,
	BOTTOM_RIGHT = 1 << 2,
	LEFT =         1 << 3,
	RIGHT =        1 << 4,
	TOP_LEFT =     1 << 5,
	TOP =          1 << 6,
	TOP_RIGHT =    1 << 7
};

const f32v2 CONNECTED_WALL_DIMS = f32v2(6.0f, 4.0f);

inline bool isBitSet(int v, NeighborBits bit) {
	return v & bit;
}

inline bool areBitsSet(int v, int bits) {
	return (v & bits) == bits;
}

ui8 getCornerIndex(NeighborBits cornerBit) {
    switch (cornerBit) {
        case BOTTOM_LEFT:
            return 0x1;
        case BOTTOM_RIGHT:
            return 0x2;
        case TOP_LEFT:
            return 0x3;
        case TOP_RIGHT:
            return 0x4;
        default:
            assert(false);
    }
    return 0;
}

bool checkLShape(int i, int lBits, ui32 lIndex, NeighborBits cornerBit) {
    if (areBitsSet(i, lBits)) {
        sConnectedWallData[i].a = lIndex;
        if (isBitSet(i, cornerBit)) {
            sConnectedWallData[i].b = getCornerIndex(cornerBit);
        }
        return true;
    }
    return false;
}

inline bool checkIShape(int i, NeighborBits iBit, ui32 iIndex, NeighborBits cornerBit1, NeighborBits cornerBit2, NeighborBits oppositeBit, ui32 oppositeIndex) {
    if (isBitSet(i, iBit)) {
        sConnectedWallData[i].a = iIndex;
        if (isBitSet(i, oppositeBit)) {
            sConnectedWallData[i].b = oppositeIndex;
        }
        else {
            if (isBitSet(i, cornerBit1)) {
                sConnectedWallData[i].b = getCornerIndex(cornerBit1);
                if (isBitSet(i, cornerBit2)) {
                    sConnectedWallData[i].c = getCornerIndex(cornerBit2);
                }
            }
            else if (isBitSet(i, cornerBit2)) {
                sConnectedWallData[i].b = getCornerIndex(cornerBit2);
            }
        }
        return true;
    }
    return false;
}

f32v4 getUvsFromConnectedWallIndex(int index) {
    const int offsetX = index % (int)CONNECTED_WALL_DIMS.x;
    const int offsetY = index / (int)CONNECTED_WALL_DIMS.x + 1;

    f32v4 uvs;
    uvs.x = offsetX / CONNECTED_WALL_DIMS.x;
    uvs.y = offsetY / CONNECTED_WALL_DIMS.y;
    uvs.z = 1.0f / CONNECTED_WALL_DIMS.x;
    uvs.w = -1.0f / CONNECTED_WALL_DIMS.y;
    return uvs;
}

// Hex map
//  0  1  2  3  4  5
//  6  7  8  9  a  b
//  c  d  e  f 10 11
// Cached after running once
void initConnectedOffsets() {
    // 1 bits represent exposed faces
	for (int i = 0; i < 256; ++i) {
		sConnectedWallData[i].data = 0;

		// Standalone
		if (areBitsSet(i, LEFT | TOP | RIGHT | BOTTOM)) {
			// No connections
			sConnectedWallData[i].a = 0x11;
			continue;
		}

		// U shapes
		if (areBitsSet(i, LEFT | TOP | RIGHT)) {
            sConnectedWallData[i].a = 0x10;
            continue;
		}
        if (areBitsSet(i, BOTTOM | TOP | RIGHT)) {
            sConnectedWallData[i].a = 0xf;
            continue;
        }
        if (areBitsSet(i, BOTTOM | TOP | LEFT)) {
            sConnectedWallData[i].a = 0xe;
            continue;
        }
        if (areBitsSet(i, LEFT | BOTTOM | RIGHT)) {
            sConnectedWallData[i].a = 0xd;
            continue;
        }

		// L shapes
        if (checkLShape(i, TOP | RIGHT, 0xc, BOTTOM_LEFT)) {
            continue;
        }
        if (checkLShape(i, TOP | LEFT, 0xb, BOTTOM_RIGHT)) {
            continue;
        }
        if (checkLShape(i, BOTTOM | RIGHT, 0xa, TOP_LEFT)) {
            continue;
        }
        if (checkLShape(i, BOTTOM | LEFT, 0x9, TOP_RIGHT)) {
            continue;
        }

		// I shapes
        if (checkIShape(i, TOP, 0x8, BOTTOM_LEFT, BOTTOM_RIGHT, BOTTOM, 0x5)) {
            continue;
        }
        if (checkIShape(i, RIGHT, 0x7, BOTTOM_LEFT, TOP_LEFT, LEFT, 0x6)) {
            continue;
        }
        if (checkIShape(i, LEFT, 0x6, BOTTOM_RIGHT, TOP_RIGHT, RIGHT, 0x7)) {
            continue;
        }
        if (checkIShape(i, BOTTOM, 0x5, TOP_LEFT, TOP_RIGHT, TOP, 0x8)) {
            continue;
        }
        
        // Finally, corners
        int j = 0;
        if (isBitSet(i, BOTTOM_LEFT)) {
            sConnectedWallData[i].dataArray[j++] = getCornerIndex(BOTTOM_LEFT);
        }
        if (isBitSet(i, BOTTOM_RIGHT)) {
            sConnectedWallData[i].dataArray[j++] = getCornerIndex(BOTTOM_RIGHT);
        }
        if (isBitSet(i, TOP_LEFT)) {
            sConnectedWallData[i].dataArray[j++] = getCornerIndex(TOP_LEFT);
        }
        if (isBitSet(i, TOP_RIGHT)) {
            sConnectedWallData[i].dataArray[j++] = getCornerIndex(TOP_RIGHT);
        }
	}
}

ChunkRenderer::ChunkRenderer(ResourceManager& resourceManager) :
	mResourceManager(resourceManager) // TODO: Remove?
{
	initConnectedOffsets();
}

ChunkRenderer::~ChunkRenderer() {
	
}

void ChunkRenderer::RenderChunk(const Chunk& chunk, const Camera2D& camera) {
	// mutable render data
	ChunkRenderData& renderData = chunk.mChunkRenderData;

	if (renderData.mBaseDirty) {
		UpdateMesh(chunk);
	}

	renderData.mBaseMesh->render(f32m4(1.0f), camera.getCameraMatrix(), &vg::SamplerState::POINT_CLAMP, &vg::DepthState::FULL);
}

void ChunkRenderer::UpdateMesh(const Chunk& chunk) {
	// mutable render data
	ChunkRenderData& renderData = chunk.mChunkRenderData;
	const i32v2& pos = chunk.getChunkPos();
	const i32v2 offset(pos.x * CHUNK_WIDTH, pos.y * CHUNK_WIDTH);

	if (!renderData.mBaseMesh) {
		renderData.mBaseMesh = std::make_unique<vg::SpriteBatch>(true, true);
	}

	Tile neighbors[8];
	renderData.mBaseMesh->begin();
	for (int y = 0; y < CHUNK_WIDTH; ++y) {
		for (int x = 0; x < CHUNK_WIDTH; ++x) {
			//  TODO: More than just ground
            TileIndex index(x, y);
			const Tile& tile = chunk.getTileAt(index);
			const SpriteData& spriteData = TileRepository::getTileData(tile.groundLayer).spriteData;
			switch (spriteData.method) {
				case TileTextureMethod::SIMPLE: {
					renderData.mBaseMesh->draw(spriteData.texture, &spriteData.uvs, f32v2(x + offset.x, y + offset.y), f32v2(spriteData.dims), color4(1.0f, 1.0f, 1.0f), 0.0f);
					break;
				}
				case TileTextureMethod::CONNECTED_WALL: {
                    chunk.getTileNeighbors(index, neighbors);
                    unsigned connectedBits = 0;
                    for (int i = 0; i < 8; ++i) {
                        if (neighbors[i].groundLayer != tile.groundLayer) {
                            connectedBits |= (1 << i);
                        }
                    }
                    const ConnectedWallData& data = sConnectedWallData[connectedBits];
                    const float verticalOffset = 0.749f;
                    if (data.data == 0) {
                        // TODO: Atlas? Starting offset?
                        f32v4 uvs(0.0f, 0.0f, 1.0f / CONNECTED_WALL_DIMS.x, -1.0f / CONNECTED_WALL_DIMS.y);
                        renderData.mBaseMesh->draw(spriteData.texture, &uvs, f32v2(x + offset.x, y + offset.y + verticalOffset), f32v2(1.0f), color4(1.0f, 1.0f, 1.0f), 1.0f);
                    }
                    else {
                        // Check if we need to render the base layer first
                        if (data.a < 0x10) {
                            f32v4 uvs(0.0f, 0.0f, 1.0f / CONNECTED_WALL_DIMS.x, -1.0f / CONNECTED_WALL_DIMS.y);
                            renderData.mBaseMesh->draw(spriteData.texture, &uvs, f32v2(x + offset.x, y + offset.y + verticalOffset), f32v2(1.0f), color4(1.0f, 1.0f, 1.0f), 0.9f);
                        }
                        // Render up to 4 textures depending on configuration
                        for (int i = 0; i < 4; ++i) {
                            const ui16 val = data.dataArray[i];
                            if (val == 0) break;

                            const f32v4 uvs = getUvsFromConnectedWallIndex(val);
                            renderData.mBaseMesh->draw(spriteData.texture, &uvs, f32v2(x + offset.x, y + offset.y + verticalOffset), f32v2(1.0f), color4(1.0f, 1.0f, 1.0f), 1.0f);
                        }
                        // Render exposed wall bottom if  needed
                        if (isBitSet(connectedBits, BOTTOM)) {
                            // Simple 2 bit LUT
                            const unsigned sideCheck = ((connectedBits & (LEFT | RIGHT)) >> 3);
                            const ui16 val = sExposedWallLookup[sideCheck];

                            const f32v4 uvs = getUvsFromConnectedWallIndex(val);
                            renderData.mBaseMesh->draw(spriteData.texture, &uvs, f32v2(x + offset.x, y + offset.y), f32v2(1.0f), color4(1.0f, 1.0f, 1.0f), 0.0f);
                        }
                        
                    }
                    break;
				}
				default:
					assert(false);
			}
		}
	}
	renderData.mBaseMesh->end();

	renderData.mBaseDirty = false;
}
