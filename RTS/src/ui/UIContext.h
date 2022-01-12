#pragma once

struct SDL_Window;
class DebugTweakerPanel;
class World;
class WorldEditor;
class Camera3D;

DECL_VG(class GBuffer);

class UIContext
{
protected:
    UIContext(World& world, const f32v2& screenResolution, SDL_Window* window);
    ~UIContext();

public:
    UIContext(UIContext& other) = delete;
    void operator=(const UIContext&) = delete;
    
    void updateEditors(const Camera3D& camera);
    void updateAndRenderUI(const vg::GBuffer* activeGBuffer, float aspectRatio);
    void renderEditorBrushDecals(const Camera3D& camera);

    static UIContext& initInstance(World& world, const f32v2& screenResolution, SDL_Window* window);
    static UIContext& getInstance();

private:

    static UIContext* sInstance;

    std::unique_ptr<DebugTweakerPanel> mDebugTweakerPanel;
    std::unique_ptr<WorldEditor> mEditor;

    SDL_Window* mWindow;
    f32v2 mScreenResolution;
};

