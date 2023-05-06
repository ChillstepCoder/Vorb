#pragma once
#include "IEditorViewportPanel.h"

class Chunk;
class IWorld;

DECL_VG(class GBuffer);

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

	VGTexture getFinalOutputTexture() override;

	void initializeWorld();

	vg::GBuffer* mActiveGBuffer = nullptr;
	std::unique_ptr<IWorld> mEditorWorld;

};

