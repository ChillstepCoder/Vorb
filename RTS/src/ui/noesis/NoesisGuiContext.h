#pragma once
class UIContext;

#include <SDL2/SDL_events.h>
#include <NsCore/Ptr.h>
#include <NsGui/IView.h>

class NoesisGuiContext {
public:
    NoesisGuiContext(UIContext& uiContext);
    ~NoesisGuiContext();

    void processInput(SDL_Event* e);
    void updateAndRender();

private:
    UIContext& mUIContext;
    Noesis::Ptr<Noesis::IView> mView;
};

extern NoesisGuiContext* sNoesisGuiContext;