#pragma once
#include <Vorb/ui/IGameScreen.h>

#include <Vorb/graphics/Texture.h>

// TODO: MOVE
#include "EntityComponentSystem.h"
#include "EntityComponentSystemRenderer.h"

class App;
class Camera2D;
class UndeadActorFactory;
class HumanActorFactory;
class PlayerActorFactory;

DECL_VG(class SpriteBatch);
DECL_VG(class SpriteFont);
DECL_VG(class TextureCache);
DECL_VUI(class InputDispatcher);
DECL_VIO(class IOManager);

class World;
class b2World;

// TODO: New file
struct DebugOptions {
	bool mWireframe = false;
};

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
	std::unique_ptr<vg::SpriteBatch> mSb;
	std::unique_ptr<vg::TextureCache> mTextureCache;
	std::unique_ptr<vg::SpriteFont> mSpriteFont;
	std::unique_ptr<World> mWorld;
	std::unique_ptr<Camera2D> mCamera2D;
	vg::Texture mCircleTexture;

	std::unique_ptr<vio::IOManager> mIoManager;


	std::unique_ptr<EntityComponentSystem> mEcs;
	std::unique_ptr<EntityComponentSystemRenderer> mEcsRenderer;

	std::unique_ptr<HumanActorFactory> mHumanActorFactory;
	std::unique_ptr<UndeadActorFactory> mUndeadActorFactory;
	std::unique_ptr<PlayerActorFactory> mPlayerActorFactory;


	float mScale = 50.0f;
	float mFps = 0.0f;

	f32v2 mTestClick = f32v2(0.0f);
	vecs::EntityID mPlayerEntity = 0;

	DebugOptions mDebugOptions;
};
