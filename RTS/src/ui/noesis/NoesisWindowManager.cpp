#include "stdafx.h"
#include "NoesisWindowManager.h"

#include <NsCore/ReflectionImplementEnum.h>


NS_IMPLEMENT_REFLECTION(WindowManager, "AM.WindowManager") {
    NsProp("ActiveWindows", &WindowManager::GetActiveWindows);
}

WindowManager::WindowManager()
{
    mActiveWindows = *new Noesis::ObservableCollection<Noesis::UserControl>();
}

Noesis::ObservableCollection<Noesis::UserControl>* WindowManager::GetActiveWindows() const
{
    return mActiveWindows;
}

void WindowManager::AddWindow(Noesis::UserControl* window)
{
    mActiveWindows->Add(window);
}

void WindowManager::RemoveWindow(Noesis::UserControl* window)
{
    mActiveWindows->Remove(window);
}