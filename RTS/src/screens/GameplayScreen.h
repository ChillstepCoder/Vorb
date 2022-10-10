#pragma once
#include <Vorb/ui/IGameScreen.h>

#include <Vorb/graphics/Texture.h>

// TODO: MOVE
#include "ecs/IEntityComponentSystem.h"
#include "network/WorldType.h"

constexpr f64 MS_PER_GAME_TICK = 40.0;
constexpr f64 MAX_MS_PER_FRAME = 80.0;

class App;
class CameraController;
class ResourceManager;
class RenderContext;
class IWorld;
class TileInteractPanel;

DECL_VUI(class InputDispatcher);


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

	void updateClient(const vui::GameTime& gameTime);
	void updateHost(const vui::GameTime& gameTime);

    void updateTimeScaling(const vui::GameTime& gameTime);
    void updateTilePicking();
    void tryUpdateAndRenderInteractPopup(const f32v3& playerPos);

	void displayLoadScreen(const nString& text);
	void initInputs();

	IWorld* mWorld = nullptr;

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

	TickingTimer mGameTimer = TickingTimer(MS_PER_GAME_TICK, MAX_MS_PER_FRAME);

	WorldType mClientType = WorldType::HOST;
	bool mIsSinglePlayer = true;

};

