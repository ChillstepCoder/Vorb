#pragma once
class UIContext;

#include <SDL2/SDL_events.h>
#include <NsCore/Ptr.h>
#include <NsGui/IView.h>

#include "util/ExclusiveCacheLine.h"
#include "ui/GameUIPanel.h"

class NoesisGuiContext {
public:
    NoesisGuiContext(UIContext& uiContext);
    ~NoesisGuiContext();

    // Returns true if event was handled
    bool processInput(SDL_Event* e);
    void updateAndRender();
    void toggleView(GameUIPanel viewName);


private:
    void initView(GameUIPanel viewName);
    // Returns true if event was handled
    bool forEachActiveViewHandleInput(std::function<bool(Noesis::IView&)> func);

    UIContext& mUIContext;
    Noesis::Ptr<Noesis::IView> mViews[e_count(GameUIPanel)];
    bool mViewWasActive[e_count(GameUIPanel)] = {};
    // Int to enable fetch_xor
    ExclusiveCacheLine<std::atomic_int> mViewWantsActive[e_count(GameUIPanel)] = {};

    std::mutex mActiveViewsMutex;
    std::vector<Noesis::IView*> mActiveViews; // Protected by mActiveViewsMutex
};

extern NoesisGuiContext* sNoesisGuiContext;