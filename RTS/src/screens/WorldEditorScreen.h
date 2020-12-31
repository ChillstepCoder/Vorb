
#include "FeatureConst.h"

#if IS_ENABLED(FEATURE_WORLD_EDITOR)

#include <Vorb/ui/IGameScreen.h>

#include <Vorb/graphics/Texture.h>

// TODO: MOVE
#include "EntityComponentSystem.h"

class App;
class Camera2D;
class ResourceManager;
class RenderContext;
class WorldEditor;

DECL_VUI(class InputDispatcher);

class World;
class b2World;

class WorldEditorScreen : public vui::IAppScreen<App>
{
public:
    WorldEditorScreen(const App* app);
    ~WorldEditorScreen();

    virtual i32 getNextScreen() const override;
    virtual i32 getPreviousScreen() const override;

    virtual void build() override;
    virtual void destroy(const vui::GameTime& gameTime) override;

    virtual void onEntry(const vui::GameTime& gameTime) override;
    virtual void onExit(const vui::GameTime& gameTime) override;

    virtual void update(const vui::GameTime& gameTime) override;
    virtual void draw(const vui::GameTime& gameTime) override;

private:

    std::unique_ptr<ResourceManager> mResourceManager;
    std::unique_ptr<World> mWorld;
    std::unique_ptr<WorldEditor> mWorldEditor;

    // Rendering
    std::unique_ptr<Camera2D> mCamera2D;
    RenderContext& mRenderContext;

    float mFps = 0.0f;

};

#endif