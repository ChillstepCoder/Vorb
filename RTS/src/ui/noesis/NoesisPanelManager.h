#pragma once
#include <NsCore/Noesis.h>
#include <NsCore/Ptr.h>
#include <NsGui/ObservableCollection.h>
#include <NsCore/ReflectionImplement.h>
#include <NsGui/UserControl.h>

#include "ui/GameUIPanel.h"
#include "ui/noesis/PanelViewModelBase.h"

#include "util/ExclusiveCacheLine.h"

class SackContainerViewModel;

class NoesisPanelManager : public NoesisApp::NotifyPropertyChangedBase {
public:
    NoesisPanelManager();
    ~NoesisPanelManager();

    static void RegisterChildren();

    static NoesisPanelManager* GetInstance();

    // Returns true if we have any active windows
    bool update();

    Noesis::ObservableCollection<PanelViewModelBase>* GetActiveWindows() const;

    void AddWindow(GameUIPanel panel);
    void RemoveWindow(GameUIPanel panel);
    void ToggleWindow(GameUIPanel panel);

    // Get specific windows
    SackContainerViewModel* GetSackContainerViewModel() const;

private:
    // Return true if panel is active
    bool UpdatePanel(GameUIPanel panel);
    void AddWindowInternal(PanelViewModelBase* window);
    void RemoveWindowInternal(PanelViewModelBase* window);

    void OnWindowCloseRequested(BaseComponent* sender, const Noesis::EventArgs&);

    Noesis::Ptr<Noesis::ObservableCollection<PanelViewModelBase>> mActiveWindows;

    bool mPanelWasActive[e_count(GameUIPanel)] = {};
    ExclusiveCacheLine<std::atomic_int> mPanelWantsActive[e_count(GameUIPanel)] = {};

    NS_DECLARE_REFLECTION(NoesisPanelManager, NoesisApp::NotifyPropertyChangedBase)
};