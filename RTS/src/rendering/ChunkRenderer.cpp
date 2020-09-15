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
	LoadShaders();
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
	for (auto& shader : mShaders) {
		shader.dispose();
	}
	mShaders.clear();
	LoadShaders();
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

void ChunkRenderer::LoadShaders()
{
    mActiveShader = 0;

	// Basic
    vg::GLProgram basicProgram = ShaderLoader::createProgramFromFile("data/shaders/standard_tile.vert", "data/shaders/standard_tile.frag");
	if (basicProgram.isLinked()) {
		mShaders.emplace_back(std::move(basicProgram));
	}

	// Depth
    vg::GLProgram depthProgram = ShaderLoader::createProgramFromFile("data/shaders/standard_depth.vert", "data/shaders/standard_depth.frag");
	if (depthProgram.isLinked()) {
		mShaders.emplace_back(std::move(depthProgram));
	}
}
