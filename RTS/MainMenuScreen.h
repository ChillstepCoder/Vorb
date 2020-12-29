#pragma once
#include <Vorb/ui/IGameScreen.h>

#include <Vorb/graphics/Texture.h>

// TODO: MOVE
#include "EntityComponentSystem.h"

class App;
class Camera2D;
class ResourceManager;
class RenderContext;

DECL_VUI(class InputDispatcher);

class World;
class b2World;

class MainMenuScreen : public vui::IAppScreen<App>
{
public:
	MainMenuScreen(const App* app);
	~MainMenuScreen();

	virtual i32 getNextScreen() const override;
	virtual i32 getPreviousScreen() const override;

	virtual void build() override;
	virtual void destroy(const vui::GameTime& gameTime) override;

	virtual void onEntry(const vui::GameTime& gameTime) override;
	virtual void onExit(const vui::GameTime& gameTime) override;

	virtual void update(const vui::GameTime& gameTime) override;
	virtual void draw(const vui::GameTime& gameTime) override;

private:

	void updateCamera(const f32v2& targetCenter, const vui::GameTime& gameTime);

    std::unique_ptr<ResourceManager> mResourceManager;
	std::unique_ptr<World> mWorld;

    // Rendering
    std::unique_ptr<Camera2D> mCamera2D;
    RenderContext& mRenderContext;

	float mTargetScale = 50.0f;
    float mScale = 50.0f;
    float mFps = 0.0f;

	f32v2 mTestClick = f32v2(0.0f);
	entt::entity mPlayerEntity = (entt::entity)0;

	// Camera
	f32v2 mTargetCameraPosition = f32v2(0.0f);
	f32v2 mCameraVelocity = f32v2(0.0f);

};

