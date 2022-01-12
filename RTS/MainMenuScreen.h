#pragma once
#include <Vorb/ui/IGameScreen.h>

#include <Vorb/graphics/Texture.h>
#include <util/Tweener.h>

// TODO: MOVE
#include "ecs/EntityComponentSystem.h"

constexpr f64 MS_PER_GAME_TICK = 40.0;
constexpr f64 MAX_MS_PER_FRAME = 80.0;

class App;
class Camera3D;
class ResourceManager;
class RenderContext;
class UIInteractMenuPopup;

DECL_VUI(class InputDispatcher);

class World;
class b2World;

#define TARGET_CAMERA_OFFSET_XY 1.0f
const f32v2 TARGET_CAMERA_NORMALS_2D[4] = {
    glm::normalize(f32v2(0.0f, -TARGET_CAMERA_OFFSET_XY)), // Cartesian::DOWN
    glm::normalize(f32v2(-TARGET_CAMERA_OFFSET_XY, 0.0f)), // Cartesian::LEFT
    glm::normalize(f32v2(TARGET_CAMERA_OFFSET_XY,  0.0f)), // Cartesian::RIGHT
    glm::normalize(f32v2(0.0f, TARGET_CAMERA_OFFSET_XY))  // Cartesian::UP
};
const f32v3 TARGET_CAMERA_NORMALS_3D[4] = {
    glm::normalize(f32v3(0.0f, -TARGET_CAMERA_OFFSET_XY, 0.0f)), // Cartesian::DOWN
    glm::normalize(f32v3(-TARGET_CAMERA_OFFSET_XY, 0.0f, 0.0f)), // Cartesian::LEFT
    glm::normalize(f32v3(TARGET_CAMERA_OFFSET_XY,  0.0f, 0.0f)), // Cartesian::RIGHT
    glm::normalize(f32v3(0.0f, TARGET_CAMERA_OFFSET_XY, 0.0f))  // Cartesian::UP
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

	void updateCamera(const vui::GameTime& gameTime);
    void updateTilePicking();
    void tryUpdateAndRenderInteractPopup(const f32v2& xyPos);

    ResourceManager* mResourceManager = nullptr;
	std::unique_ptr<World> mWorld;

    // Rendering
    std::unique_ptr<Camera3D> mCamera3D;
    RenderContext& mRenderContext;

    float mFps = 0.0f;

	entt::entity mPlayerEntity = (entt::entity)0;

	// Camera
    // TODO: 3D
    Cartesian mCameraCartesianDirection = Cartesian::UP;
    Tweener<f32v3> mCameraPositionTweener = Tweener<f32v3>(f32v3(0.0f));
	SphericalTweener<f32v3> mCameraDirectionTweener = SphericalTweener<f32v3>(TARGET_CAMERA_NORMALS_3D[enum_cast(Cartesian::UP)], 0.4f/*speed*/, 0.2f/*acceleration*/);
	f32 mCameraDirectionZOffset = -0.3f;
	
	// Pathfinding test
	ui32v2 mPathFindStart = ui32v2(0);
	bool mIsPathfinding = false;

    // UI
	f32v2 mSelectedTilePosition = f32v2(0.0f);
	std::unique_ptr<UIInteractMenuPopup> mRightClickInteractPopup;
	f32v2 mMousePosition = f32v2(0.0f);

	TickingTimer mGameTimer = TickingTimer(MS_PER_GAME_TICK, MAX_MS_PER_FRAME);

};

