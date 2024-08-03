#pragma once

#include <NsApp/EventTriggerBase.h>
#include <NsGui/UIElement.h>

class EventTriggerWithModifier : public NoesisApp::EventTriggerBaseT<Noesis::UIElement> {
public:
    EventTriggerWithModifier();
    EventTriggerWithModifier(const char* eventName);
    ~EventTriggerWithModifier();

    const char* GetEventName() const override;
    void SetEventName(const char* name);

    // Hides Freezable methods for convenience
    //@{
    Noesis::Ptr<EventTriggerWithModifier> Clone() const;
    Noesis::Ptr<EventTriggerWithModifier> CloneCurrentValue() const;
    //@}


    Noesis::ModifierKeys GetModifiers() const;
    void SetModifiers(Noesis::ModifierKeys modifiers);

public:
    inline static const Noesis::DependencyProperty* EventNameProperty;
    inline static const Noesis::DependencyProperty* ModifiersProperty;

private:
    Noesis::UIElement* mSource;

    bool CheckModifiers() const;
    void OnEvent(const Noesis::EventArgs& args) override;

    Noesis::Ptr<Noesis::Freezable> CreateInstanceCore() const override;

    NS_DECLARE_REFLECTION(EventTriggerWithModifier, EventTriggerBaseT)
};

