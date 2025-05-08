#include "stdafx.h"
#include "EventTriggerWithModifier.h"

#include <NsGui/UIElementData.h>
#include <NsGui/Keyboard.h>
#include <NsGui/TypeConverterMetaData.h>
#include <NsCore/ReflectionImplement.h>
#include <NsCore/ReflectionImplementEnum.h>

EventTriggerWithModifier::EventTriggerWithModifier() : mSource(0) {

}

EventTriggerWithModifier::EventTriggerWithModifier(const char* eventName) : mSource(0) {
    ForceCreateDependencyProperties();
    SetEventName(eventName);
}

EventTriggerWithModifier::~EventTriggerWithModifier() = default;

const char* EventTriggerWithModifier::GetEventName() const {
    return GetValue<Noesis::String>(EventNameProperty).Str();
}

void EventTriggerWithModifier::SetEventName(const char* name) {
    SetValue<Noesis::String>(EventNameProperty, name);
}

Noesis::Ptr<EventTriggerWithModifier> EventTriggerWithModifier::Clone() const {
    return Noesis::StaticPtrCast<EventTriggerWithModifier>(Freezable::Clone());
}

Noesis::Ptr<EventTriggerWithModifier> EventTriggerWithModifier::CloneCurrentValue() const {
    return Noesis::StaticPtrCast<EventTriggerWithModifier>(Freezable::CloneCurrentValue());
}

Noesis::ModifierKeys EventTriggerWithModifier::GetModifiers() const {
    return GetValue<Noesis::ModifierKeys>(ModifiersProperty);
}

void EventTriggerWithModifier::SetModifiers(Noesis::ModifierKeys modifiers) {
    SetValue<Noesis::ModifierKeys>(ModifiersProperty, modifiers);
}

Noesis::UIElement* GetRoot(Noesis::UIElement* current) {
    Noesis::UIElement* root = 0;

    while (current != 0) {
        root = current;
        current = current->GetUIParent();
    }

    return root;
}

bool EventTriggerWithModifier::CheckModifiers() const {
    assert(GetSource());
    Noesis::Keyboard* keyboard = GetSource()->GetKeyboard();
    return GetModifiers() == keyboard->GetModifiers();
}

void EventTriggerWithModifier::OnEvent(const Noesis::EventArgs& args) {
    if (CheckModifiers()) {
        ParentClass::OnEvent(args);
    }
}

Noesis::Ptr<Noesis::Freezable> EventTriggerWithModifier::CreateInstanceCore() const {
    return *new EventTriggerWithModifier();
}

NS_IMPLEMENT_REFLECTION(EventTriggerWithModifier, "AM.EventTriggerWithModifier") {
    Noesis::DependencyData* data = NsMeta<Noesis::DependencyData>(Noesis::TypeOf<SelfClass>());
    data->RegisterProperty<Noesis::String>(EventNameProperty, "EventName",
        Noesis::PropertyMetadata::Create(Noesis::String("Loaded"), Noesis::PropertyChangedCallback(
            [](DependencyObject* d,
                const Noesis::DependencyPropertyChangedEventArgs& e)
    {
        EventTriggerWithModifier* trigger = static_cast<EventTriggerWithModifier*>(d);
        const Noesis::String& oldName = e.OldValue<Noesis::String>();
        const Noesis::String& newName = e.NewValue<Noesis::String>();
        trigger->OnEventNameChanged(oldName.Str(), newName.Str());
    })));

    NsProp("Modifiers", &EventTriggerWithModifier::GetModifiers, &EventTriggerWithModifier::SetModifiers)
        .Meta<Noesis::TypeConverterMetaData>("Converter<ModifierKeys>");

    data->RegisterProperty<Noesis::ModifierKeys>(ModifiersProperty, "Modifiers",
        Noesis::PropertyMetadata::Create(Noesis::ModifierKeys_None));
}