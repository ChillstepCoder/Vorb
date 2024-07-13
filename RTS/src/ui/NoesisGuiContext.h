#pragma once
class UIContext;

class NoesisGuiContext {
public:
    NoesisGuiContext(UIContext& uiContext);

private:
    UIContext& mUIContext;
};

