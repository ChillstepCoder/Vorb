#pragma once
#include <NsCore/Noesis.h>
#include <NsCore/Ptr.h>
#include <NsGui/ObservableCollection.h>
#include <NsCore/ReflectionImplement.h>
#include <NsGui/UserControl.h>

#include "ui/GameUIPanel.h"
#include "ui/noesis/WindowViewModelBase.h"

#include "util/ExclusiveCacheLine.h"

class SackContainerViewModel;

class NoesisWindowManager : public NoesisApp::NotifyPropertyChangedBase {
public:
    NoesisWindowManager();
    ~NoesisWindowManager();

    static void RegisterChildren();

    static NoesisWindowManager* GetInstance();

    // Returns true if we have any active windows
    bool update();

    Noesis::ObservableCollection<WindowViewModelBase>* GetActiveWindows() const;

    void AddWindow(GameUIPanel panel);
    void RemoveWindow(GameUIPanel panel);
    void ToggleWindow(GameUIPanel panel);

    // Get specific windows
    SackContainerViewModel* GetSackContainerViewModel() const;

private:
    // Return true if panel is active
    bool UpdatePanel(GameUIPanel panel);
    void AddWindowInternal(WindowViewModelBase* window);
    void RemoveWindowInternal(WindowViewModelBase* window);

    void OnWindowCloseRequested(BaseComponent* sender, const Noesis::EventArgs&);

    Noesis::Ptr<Noesis::ObservableCollection<WindowViewModelBase>> mActiveWindows;

    bool mPanelWasActive[e_count(GameUIPanel)] = {};
    ExclusiveCacheLine<std::atomic_int> mPanelWantsActive[e_count(GameUIPanel)] = {};

    NS_DECLARE_REFLECTION(NoesisWindowManager, NoesisApp::NotifyPropertyChangedBase)
};