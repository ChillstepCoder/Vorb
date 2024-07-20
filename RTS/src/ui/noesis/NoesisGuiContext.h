#pragma once
class UIContext;

#include <NsCore/Ptr.h>

#include "util/ExclusiveCacheLine.h"
#include "ui/GameUIPanel.h"


#include "ecs/component/ThreadSharedComponent.h"

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

    // UI Windows
    void updateItemSackUI(RenderThreadSharedComponentDataPtr data);

private:
    void initView(GameUIPanel viewName);
    // Returns true if event was handled
    bool forEachActiveViewHandleInput(std::function<bool(Noesis::IView&)> func);
    void onPanelActiveChanged(GameUIPanel panel, bool active);

    UIContext& mUIContext;
    Noesis::Ptr<Noesis::IView> mViews[e_count(GameUIPanel)];
    bool mViewWasActive[e_count(GameUIPanel)] = {};
    // Int to enable fetch_xor
    ExclusiveCacheLine<std::atomic_int> mViewWantsActive[e_count(GameUIPanel)] = {};

    std::mutex mActiveViewsMutex;
    std::vector<Noesis::IView*> mActiveViews; // Protected by mActiveViewsMutex

    // UI Windows
    RenderThreadSharedComponentDataPtr mItemSackData;
};

extern NoesisGuiContext* sNoesisGuiContext;