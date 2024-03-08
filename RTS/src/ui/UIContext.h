#pragma once

#include "ui/UIContextEvents.h"

struct SDL_Window;
class Camera3D;
class TileInspectionPanel;
struct TileHandle;
class PauseMenuPanel;
class EditorRoot;
class LocalMinigameContext;

DECL_VG(class GBuffer);

class UIContext
{
protected:
    UIContext(const f32v2& screenResolution, SDL_Window* window);
    ~UIContext();

public:
    UIContext(UIContext& other) = delete;
    void operator=(const UIContext&) = delete;
    
    void updateEditors(World* world, const Camera3D& camera, const f32v3& mousePickRay);
    void updateAndRenderUI(const vg::GBuffer* activeGBuffer, f32 elapsedSec, const Camera3D& camera);
    void renderEditorBrushDecals(const Camera3D& camera);

    void activateTileInspectionPanel(const f32v2& screenPos, const TileHandle& tileHandle);
    void closeTileInspectionPanel();

    bool isEditorCameraActive();
    f32v3 getEditorCameraPosition();
    f32v3 getEditorCameraDirection();
    f32v3 getEditorCameraRight();
    f32v3 getEditorCameraUp();
    void toggleMainMenu();

    static UIContext& initInstance(const f32v2& screenResolution, SDL_Window* window);
    static UIContext& getInstance();

    static ui32v2 getWindowDims();

    bool shouldPauseGameRendering() const;

    LocalMinigameContext& getMinigameContext() const { return *mMinigameContext; }
    EditorRoot& getEditorRoot() const { return *mEditorRoot; }

    EVENT_LISTENER_FUNCS(UIContext, EditorWorldSet, UIContextEventType::EditorWorldSet, const UIContextEvent&);
private:

    static UIContext* sInstance;

    std::unique_ptr<EditorRoot> mEditorRoot;
    std::unique_ptr<TileInspectionPanel> mTileInspectionPanel;
    std::unique_ptr<PauseMenuPanel> mPauseMenuPanel;
    std::unique_ptr<LocalMinigameContext> mMinigameContext;

    SDL_Window* mWindow;
    f32v2 mScreenResolution;

    EVENT_DISPATCHER_DEF(UIContext);
};

