#pragma once
class UIContext;

#include <NsCore/Ptr.h>

#include "util/ExclusiveCacheLine.h"
#include "ui/GameUIPanel.h"

#include "ecs/component/ThreadSharedComponent.h"

class NoesisPanelManager;

#pragma region forward_declarations
union SDL_Event;
namespace Noesis {
    NS_INTERFACE IView;
}
#pragma endregion

class NoesisGuiContext {
public:
    NoesisGuiContext(UIContext& uiContext);
    ~NoesisGuiContext();

    // Returns true if event was handled
    bool processInput(SDL_Event* e);
    void updateAndRender();
    void toggleView(GameUIPanel viewName);
    void disableView(GameUIPanel viewName);
    void enableView(GameUIPanel viewName);

    // UI Windows
    void addRenderThreadSharedComponentData(RenderThreadSharedComponentDataPtr data);

private:
    void initView();
    void updateSackUI();

    UIContext& mUIContext;
    Noesis::Ptr<Noesis::IView> mView;

    // UI Window data
    std::map<RenderThreadSharedComponentType, RenderThreadSharedComponentDataPtr> mRenderThreadSharedComponentData;
};

extern NoesisGuiContext* sNoesisGuiContext;