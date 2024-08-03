#pragma once

#include <NsApp/NotifyPropertyChangedBase.h>
#include <NsCore/ReflectionDeclare.h>
#include <NsCore/String.h>
#include <NsCore/ReflectionDeclareEnum.h>
#include <NsApp/DelegateCommand.h>

#include "ui/GameUIPanel.h"


class PanelViewModelBase : public NoesisApp::NotifyPropertyChangedBase {
public:
    PanelViewModelBase(GameUIPanel panel);
    virtual ~PanelViewModelBase() = default;

    // Common properties for all windows
    float GetX() const { return mX; }
    void SetX(float x) { mX = x; OnPropertyChanged("X"); }

    float GetY() const { return mY; }
    void SetY(float y) { mY = y; OnPropertyChanged("Y"); }

    float GetMinWidth() const { return mMinWidth; }
    void SetMinWidth(float width) { mMinWidth = width; OnPropertyChanged("MinWidth"); }

    float GetMinHeight() const { return mMinHeight; }
    void SetMinHeight(float height) { mMinHeight = height; OnPropertyChanged("MinHeight"); }

    float GetMaxWidth() const { return mMaxWidth; }
    void SetMaxWidth(float width) { mMaxWidth = width; OnPropertyChanged("MaxWidth"); }

    float GetMaxHeight() const { return mMaxHeight; }
    void SetMaxHeight(float height) { mMaxHeight = height; OnPropertyChanged("MaxHeight"); }

    float GetAvailableWidth() const { return mAvailableWidth; }
    void SetAvailableWidth(float width) { mAvailableWidth = width; OnPropertyChanged("AvailableWidth"); }

    float GetAvailableHeight() const { return mAvailableHeight; }
    void SetAvailableHeight(float height) { mAvailableHeight = height; OnPropertyChanged("AvailableHeight"); }

    const NoesisApp::DelegateCommand* GetCloseCommand() const { return mCloseCommand; }

    Noesis::EventHandler& CloseRequested() { return mCloseRequested; }

    GameUIPanel GetWindowType() const { return mPanel; }

protected:
    void Close(BaseComponent* param) {
        onClose();
        mCloseRequested(this, Noesis::EventArgs::Empty);
    }
    virtual void onClose() {}

    float mX = 0.0f;
    float mY = 0.0f;
    float mMinWidth = -1.0f;
    float mMinHeight = -1.0f;
    float mMaxWidth = -1.0f;
    float mMaxHeight = -1.0f;
    float mAvailableWidth = -1.0f;
    float mAvailableHeight = -1.0f;
    Noesis::Ptr<NoesisApp::DelegateCommand> mCloseCommand;
    Noesis::EventHandler mCloseRequested;
    GameUIPanel mPanel;

    NS_DECLARE_REFLECTION(PanelViewModelBase, NoesisApp::NotifyPropertyChangedBase)
};