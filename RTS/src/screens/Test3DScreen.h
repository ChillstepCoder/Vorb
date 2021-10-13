#pragma once

#include "FeatureConst.h"

#if IS_ENABLED(FEATURE_TEST_3D)

#include <Vorb/ui/IGameScreen.h>

#include <Vorb/graphics/Texture.h>

#include "camera/Camera3D.h"

#include "rendering/QuadMesh.h"

#include "rendering/RenderContext.h"

class App;
class ResourceManager;
class World;

class Test3DScreen : public vui::IAppScreen<App> {
public:
	Test3DScreen(const App* app);
    ~Test3DScreen();

	i32 getNextScreen() const override;
	i32 getPreviousScreen() const override;

	void build() override;
	void destroy(const vui::GameTime& gameTime) override;

	void onEntry(const vui::GameTime& gameTime) override;
	void onExit(const vui::GameTime& gameTime) override;

	void update(const vui::GameTime& gameTime) override;
	void draw(const vui::GameTime& gameTime) override;
private:
    ResourceManager* mResourceManager = nullptr;
    std::unique_ptr<World> mWorld;

	Camera3D mCamera;
    RenderContext& mRenderContext;
	std::unique_ptr<QuadMesh> mQuadMesh;
};

#endif