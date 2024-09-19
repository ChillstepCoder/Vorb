#pragma once
#include "ui/editor/AssetEditorViewportPanel.h"

#include "definitions/BiomeDef.h"

class LocalChunk;
class World;
class EditorWorldInterfaceController;
class HostWorldData;

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

	void setCurrentAsset(AssetID assetId) override;

protected:
	void renderCenterPanel(i32AABB2* outImageRect) override;
	void postCenterPanelRender(const i32AABB2& imageRect) override;

	VGTexture getFinalOutputTexture() override;

	void initializeWorld();
	void generateHeightmap(HostWorldData& worldData);
	void initializeController();
	void updateActiveEditorWorld(World* world);
	void resetCamera();

	vg::GBuffer* mActiveGBuffer = nullptr;
    std::unique_ptr<World> mEditorWorld;
    std::unique_ptr<EditorWorldInterfaceController> mWorldInterfaceController;
	bool mLeftMousePressed = false;
	bool mShuttingDownWorld = false;
	f32 mWorldSeed = 18424.0f;

	VGTexture mBiomeTexture;
	VGBuffer mHeightSSBO = 0;
	GLfloat* mMappedHeights = nullptr;
	GLsync mFence = 0;
	f32 mStartHeight = 0.f;
	f32 mHeightOffset = 0.f;
	bool mFirstEntry = true;
};

