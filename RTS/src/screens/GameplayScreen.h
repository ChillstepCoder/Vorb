#pragma once
#include <Vorb/ui/IGameScreen.h>

#include <Vorb/graphics/Texture.h>

// TODO: MOVE
#include "ecs/IEntityComponentSystem.h"
#include "network/WorldNetMode.h"

#include <Vorb/Timing.h>

#include <Vorb/ui/KeyboardEventManager.h>

#include "world/WorldObjectQuery.h"

constexpr f64 MS_PER_GAME_TICK = 40.0;
constexpr f64 MAX_MS_PER_FRAME = 80.0;

class App;
class CameraController;
class ResourceManager;
class RenderContext;
class IWorld;
class TileInteractPanel;
class DeferredPhysicsPick;

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
	void initWorld();
	void initCamera();

	void updateClient(const vui::GameTime& gameTime);
	void updateHost(const vui::GameTime& gameTime);
	void updateScreen();

    void updateTimeScaling(const vui::GameTime& gameTime);
    void updateTilePicking();
    void tryUpdateAndRenderInteractPopup();

	void displayLoadScreen(const nString& text, bool syncWindow);
	void initInputs();

	// TODO: Maybe shouldn't live on the screen?
	std::unique_ptr<IWorld> mWorld;

    // Rendering
    std::unique_ptr<CameraController> mCameraController;
	ResourceManager& mResourceManager;
	RenderContext* mRenderContext = nullptr;
    float mFps = 0.0f;
	
	// Pathfinding test
	ui32v2 mPathFindStart = ui32v2(0);
	bool mIsPathfinding = false;

    // UI
	TileHandle mSelectedTileHandle;
	f32v2 mSelectedScreenPos = f32v2(0.0f);
    std::unique_ptr<TileInteractPanel> mRightClickInteractPopup;
	f32v2 mMousePosition = f32v2(0.0f);
	PreciseTimer mRightClickTimer;
    f32v3 mRightClickPickPos = f32v3(FLT_MAX);
    f32v3 mMousePickRay = f32v3(0.0f);
	WorldObjectQueryPtr mWorldObjectQuery;
	bool mIsQuerying = false;

	WorldNetMode mNetMode = WorldNetMode::Host;

	GameplayScreenState mState = GameplayScreenState::INIT;
	vui::MouseListeners mMouseListeners;
	vui::KeyListeners mKeyListeners;

    std::unique_ptr<DeferredPhysicsPick> mRightClickDownPick;
    std::unique_ptr<DeferredPhysicsPick> mRightClickUpPick;
	f32v2 mRightClickUpPickScreenPos = f32v2(0.0);

};

