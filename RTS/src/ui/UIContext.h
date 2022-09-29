#pragma once

struct SDL_Window;
class DebugTweakerPanel;
class WorldEditor;
class Camera3D;
class TileInspectionPanel;
struct TileHandle;
class EntityComponentSystem;

DECL_VG(class GBuffer);

class UIContext
{
protected:
    UIContext(const f32v2& screenResolution, SDL_Window* window);
    ~UIContext();

public:
    UIContext(UIContext& other) = delete;
    void operator=(const UIContext&) = delete;
    
    void updateEditors(const Camera3D& camera);
    void updateAndRenderUI(EntityComponentSystem& ecs, const vg::GBuffer* activeGBuffer, float aspectRatio);
    void renderEditorBrushDecals(const Camera3D& camera);

    void activateTileInspectionPanel(const f32v2& screenPos, const TileHandle& tileHandle);
    void closeTileInspectionPanel();

    static UIContext& initInstance(const f32v2& screenResolution, SDL_Window* window);
    static UIContext& getInstance();

private:

    static UIContext* sInstance;

    std::unique_ptr<DebugTweakerPanel> mDebugTweakerPanel;
    std::unique_ptr<WorldEditor> mEditor;
    std::unique_ptr<TileInspectionPanel> mTileInspectionPanel;

    SDL_Window* mWindow;
    f32v2 mScreenResolution;
};

