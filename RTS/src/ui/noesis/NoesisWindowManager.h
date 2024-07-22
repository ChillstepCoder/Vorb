#pragma once
#include <NsCore/Noesis.h>
#include <NsGui/ObservableCollection.h>
#include <NsCore/ReflectionImplement.h>
#include <NsGui/UserControl.h>

class WindowManager : public Noesis::BaseComponent
{
public:
    WindowManager();

    Noesis::ObservableCollection<Noesis::UserControl>* GetActiveWindows() const;
    void AddWindow(Noesis::UserControl* window);
    void RemoveWindow(Noesis::UserControl* window);

private:
    Noesis::Ptr<Noesis::ObservableCollection<Noesis::UserControl>> mActiveWindows;

    NS_DECLARE_REFLECTION(WindowManager, BaseComponent)
};