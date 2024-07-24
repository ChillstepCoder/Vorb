#pragma once

#include <NsApp/NotifyPropertyChangedBase.h>
#include <NsCore/ReflectionDeclare.h>
#include <NsCore/String.h>
#include <NsCore/ReflectionDeclareEnum.h>
#include <NsApp/DelegateCommand.h>

#include "ui/GameUIPanel.h"

#define IMPLEMENT_WINDOW_BASE_REFLECTION(MyClass) \
        NsProp("X", &MyClass::GetX, &MyClass::SetX); \
        NsProp("Y", &MyClass::GetY, &MyClass::SetY); \
        NsProp("CloseCommand", &MyClass::GetCloseCommand); \
        NsProp("WindowType", &MyClass::GetWindowType);

class WindowViewModelBase : public NoesisApp::NotifyPropertyChangedBase {
public:
    WindowViewModelBase(GameUIPanel panel);
    virtual ~WindowViewModelBase() = default;

    // Common properties for all windows
    float GetX() const { return mX; }
    void SetX(float x) { mX = x; OnPropertyChanged("X"); }

    float GetY() const { return mY; }
    void SetY(float y) { mY = y; OnPropertyChanged("Y"); }

    const NoesisApp::DelegateCommand* GetCloseCommand() const { return mCloseCommand; }

    Noesis::EventHandler& CloseRequested() { return mCloseRequested; }

    GameUIPanel GetWindowType() const { return mPanel; }

protected:
    virtual void Close(BaseComponent* param) {
        mCloseRequested(this, Noesis::EventArgs::Empty);
    }

private:
    float mX = 0.0f;
    float mY = 0.0f;
    Noesis::String mTitle;
    Noesis::Ptr<NoesisApp::DelegateCommand> mCloseCommand;
    Noesis::EventHandler mCloseRequested;
    GameUIPanel mPanel;

    NS_DECLARE_REFLECTION(WindowViewModelBase, NoesisApp::NotifyPropertyChangedBase)
};