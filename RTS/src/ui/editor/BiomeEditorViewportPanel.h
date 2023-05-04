#pragma once
#include "IEditorViewportPanel.h"

class Chunk;
class IWorld;

class BiomeEditorViewportPanel : public IEditorViewportPanel
{
public:
	BiomeEditorViewportPanel();
	~BiomeEditorViewportPanel();
	bool updateAndRender() override;
	void updateAndRenderControls(f32 ySize) override;

    void onEnter() override;
    void onExit() override;

protected:
	const MaterialShader* getShader() override;
	void uploadCustomShaderUniforms(const MaterialShader* shader, ui32 availableTextureUnit) override;
	void renderMesh() override;

	void initializeWorld();

	std::unique_ptr<IWorld> mEditorWorld;

};

