#pragma once
#include "IEditorViewportPanel.h"

class Chunk;

class BiomeEditorViewportPanel : public IEditorViewportPanel
{
public:
	BiomeEditorViewportPanel();
	~BiomeEditorViewportPanel();
	bool updateAndRender() override;
	void updateAndRenderControls(f32 ySize) override;

protected:
	const MaterialShader* getShader() override;
	void uploadCustomShaderUniforms(const MaterialShader* shader, ui32 availableTextureUnit) override;
	void renderMesh() override;

	void initializeChunks();

	std::unique_ptr<Chunk[]> mChunks;

};

