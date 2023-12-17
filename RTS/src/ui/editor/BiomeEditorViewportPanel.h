#pragma once
#include "IEditorViewportPanel.h"

#include "definitions/BiomeDef.h"

class Chunk;
class World;
class EditorWorldInterfaceController;

DECL_VG(class GBuffer);

class BiomeEditorViewportPanel : public IEditorViewportPanel
{
public:
	BiomeEditorViewportPanel();
	~BiomeEditorViewportPanel();
	bool updateAndRender(f32 elapsedSec) override;
	void updateAndRenderPrimaryControls(f32 ySize) override;

    void onEnter() override;
    void onExit() override;

protected:
	void renderCenterPanel(i32AABB2* outImageRect) override;

	VGTexture getFinalOutputTexture() override;

	void initializeWorld();

	vg::GBuffer* mActiveGBuffer = nullptr;
    std::unique_ptr<World> mEditorWorld;
    std::unique_ptr<EditorWorldInterfaceController> mWorldInterfaceController;
	bool mLeftMousePressed = false;

	BiomeUniqueID mSelectedBiome = BiomeUniqueID::Plains;

	VGTexture mBiomeTexture;
};

