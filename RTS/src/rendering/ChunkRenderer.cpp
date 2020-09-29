#include "stdafx.h"
#include "ChunkRenderer.h"
#include "TileSet.h"
#include "Camera2D.h"
#include "world/Chunk.h"
#include "ResourceManager.h"
#include "rendering/ChunkMesher.h"
#include "rendering/QuadMesh.h"
#include "rendering/BasicVertex.h"
#include "rendering/ShaderLoader.h"

#include <Vorb/graphics/SpriteBatch.h>
#include <Vorb/graphics/SamplerState.h>
#include <Vorb/graphics/DepthState.h>

ChunkRenderer::ChunkRenderer(ResourceManager& resourceManager) :
	mResourceManager(resourceManager) // TODO: Remove?
{
    mMesher = std::make_unique<ChunkMesher>(resourceManager.getTextureAtlas());
}

ChunkRenderer::~ChunkRenderer() {
	
}

void ChunkRenderer::RenderChunk(const Chunk& chunk, const Camera2D& camera) {
	// mutable render data
	ChunkRenderData& renderData = chunk.mChunkRenderData;

	if (renderData.mBaseDirty) {
        PreciseTimer timer;
		UpdateMesh(chunk);
        std::cout << "Mesh updated in " << timer.stop() << " ms\n";
	}

	renderData.mChunkMesh->draw(camera, mShaders[mActiveShader]);
}

void ChunkRenderer::ReloadShaders() {
	// reload dirty via timestamp
	assert(false);
	for (auto& shader : mShaders) {
		shader.dispose();
	}
	mShaders.clear();
	InitPostLoad();
}

void ChunkRenderer::SelectNextShader() {
	++mActiveShader;
	if (mActiveShader >= mShaders.size()) {
		mActiveShader = 0;
	}
}

void ChunkRenderer::UpdateMesh(const Chunk& chunk) {
	mMesher->createMesh(chunk);
}

void ChunkRenderer::InitPostLoad()
{
    mActiveShader = 0;

	// Basic
    vg::GLProgram basicProgram = ShaderLoader::getProgram("standard_tile");
	if (basicProgram.isLinked()) {
		mShaders.emplace_back(std::move(basicProgram));
	}

	// Depth
    vg::GLProgram depthProgram = ShaderLoader::getProgram("standard_depth");
	if (depthProgram.isLinked()) {
		mShaders.emplace_back(std::move(depthProgram));
	}
}
