#pragma once
#include <Vorb/ui/IGameScreen.h>

#include <Vorb/graphics/Texture.h>

// TODO: MOVE
#include "ecs/IFullECS.h"
#include "network/WorldNetMode.h"

#include <Vorb/Timing.h>
#include <Vorb/ui/KeyboardEventManager.h>

#include "world/WorldObjectQuery.h"

#include "ui/UIContextEvents.h"

constexpr f64 MS_PER_GAME_TICK = 40.0;
constexpr f64 MAX_MS_PER_FRAME = 80.0;

class App;
class CameraController;
class ResourceManager;
class RenderContext;
class World;
class TileInteractPanel;
class DeferredPhysicsPick;
class IWorldInterfaceController;

DECL_VUI(class InputDispatcher);


enum class GameplayScreenState {
	INIT,
	WAITING_JOIN_SERVER,
	RUNNING,
};

class GameplayScreen : public vui::IAppScreen<App>
{
public:
	GameplayScreen(App* const app);
	~GameplayScreen();

	virtual i32 getNextScreen() const override;
	virtual i32 getPreviousScreen() const override;

	virtual void build() override;
	virtual void destroy(const vui::GameTime& gameTime) override;

	virtual void onEntry(const vui::GameTime& gameTime) override;
	virtual void onExit(const vui::GameTime& gameTime) override;

	virtual void update(const vui::GameTime& gameTime) override;

    virtual void draw(const vui::GameTime& gameTime) override;

private:
	void initCamera();
	void initEvents();
	void initWorldInterfaceController();

	void updateClient(const vui::GameTime& gameTime);
	void updateHost(const vui::GameTime& gameTime);

    void updateTimeScaling(const vui::GameTime& gameTime);

	void displayLoadScreen(const nString& text, bool syncWindow);

    // Rendering
    std::unique_ptr<CameraController> mCameraController;
	ResourceManager& mResourceManager;
	RenderContext* mRenderContext = nullptr;
    float mFps = 0.0f;

	// Controller
	std::unique_ptr<IWorldInterfaceController> mWorldInterfaceController;

	WorldNetMode mNetMode = WorldNetMode::Host;

	GameplayScreenState mState = GameplayScreenState::INIT;
	UIContextListeners mUIListeners;

};

