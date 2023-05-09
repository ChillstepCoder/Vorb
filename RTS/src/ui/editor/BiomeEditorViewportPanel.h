#pragma once
#include "IEditorViewportPanel.h"

class Chunk;
class IWorld;
class IWorldInterfaceController;

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
	void renderCenterPanel() override;

	VGTexture getFinalOutputTexture() override;

	void initializeWorld();

	vg::GBuffer* mActiveGBuffer = nullptr;
    std::unique_ptr<IWorld> mEditorWorld;
    std::unique_ptr<IWorldInterfaceController> mWorldInterfaceController;

};

