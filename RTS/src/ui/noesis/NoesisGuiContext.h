#pragma once
class UIContext;

#include <SDL2/SDL_events.h>
#include <NsCore/Ptr.h>
#include <NsGui/IView.h>

enum class NoesisGuiView {
    Inventory,
    COUNT
};

class NoesisGuiContext {
public:
    NoesisGuiContext(UIContext& uiContext);
    ~NoesisGuiContext();

    void processInput(SDL_Event* e);
    void updateAndRender();
    void addView(NoesisGuiView viewName);
    void removeView(NoesisGuiView viewName);
    void toggleView(NoesisGuiView viewName);

private:
    UIContext& mUIContext;
    FlatMap<NoesisGuiView, Noesis::Ptr<Noesis::IView>> mViews;
};

extern NoesisGuiContext* sNoesisGuiContext;