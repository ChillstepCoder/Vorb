#pragma once
class UIContext;

#include <NsCore/Ptr.h>

#include "util/ExclusiveCacheLine.h"
#include "ui/GameUIPanel.h"

#include "ecs/component/ThreadSharedComponent.h"

class NoesisWindowManager;

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
    void updateItemSackUI(RenderThreadSharedComponentDataPtr data);

private:
    void initView();

    UIContext& mUIContext;
    Noesis::Ptr<Noesis::IView> mView;
    // Int to enable fetch_xor
    NoesisWindowManager* mWindowManager = nullptr;

    // UI Windows
    RenderThreadSharedComponentDataPtr mItemSackData;
};

extern NoesisGuiContext* sNoesisGuiContext;