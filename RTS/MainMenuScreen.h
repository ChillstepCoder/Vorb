#pragma once

#include "stdafx.h"
#include <Vorb/ui/IGameScreen.h>

#include <Vorb/graphics/Texture.h>

// TODO: MOVE
#include "EntityComponentSystem.h"
#include "EntityComponentSystemRenderer.h"

class App;
class Camera2D;

DECL_VG(class SpriteBatch);
DECL_VG(class SpriteFont);
DECL_VG(class TextureCache);
DECL_VUI(class InputDispatcher);
DECL_VIO(class IOManager);

class TileGrid;

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
	std::unique_ptr<vg::SpriteBatch> m_sb;
	std::unique_ptr<vg::TextureCache> m_textureCache;
	std::unique_ptr<vg::SpriteFont> mSpriteFont;
	std::unique_ptr<TileGrid> m_tileGrid;
	std::unique_ptr<Camera2D> m_camera2D;
	vg::Texture m_circleTexture;

	std::unique_ptr<vio::IOManager> m_ioManager;

	EntityComponentSystem mEntityComponentSystem;
	std::unique_ptr<EntityComponentSystemRenderer> mEcsRenderer;

	float m_scale = 1.0f;
	float m_fps = 0.0f;

	f32v2 m_testClick = f32v2(0.0f);
};

