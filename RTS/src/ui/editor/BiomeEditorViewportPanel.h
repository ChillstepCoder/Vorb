#pragma once
#include "ui/editor/AssetEditorViewportPanel.h"

#include "definitions/BiomeDef.h"

class Chunk;
class World;
class EditorWorldInterfaceController;

DECL_VG(class GBuffer);

class BiomeEditorViewportPanel : public AssetEditorViewportPanel<BiomeDef>
{
public:
	BiomeEditorViewportPanel();
	~BiomeEditorViewportPanel();
	void updateAndRenderInternal(f32 elapsedSec) override;
	void updateAndRenderPrimaryControls(f32 ySize) override;

    const char* getViewportWindowName() const override { return "Biome Editor"; }

    void onEnter() override;
    void onExit() override;

protected:
	void renderCenterPanel(i32AABB2* outImageRect) override;
	void postCenterPanelRender(const i32AABB2& imageRect) override;

	VGTexture getFinalOutputTexture() override;

	void initializeWorld();

	vg::GBuffer* mActiveGBuffer = nullptr;
    std::unique_ptr<World> mEditorWorld;
    std::unique_ptr<EditorWorldInterfaceController> mWorldInterfaceController;
	bool mLeftMousePressed = false;

	BiomeUniqueID mSelectedBiome = BiomeUniqueID::Plains;

	VGTexture mBiomeTexture;
};

