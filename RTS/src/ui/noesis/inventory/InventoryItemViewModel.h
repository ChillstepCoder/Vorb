#pragma once

#include <NsApp/NotifyPropertyChangedBase.h>
#include <NsCore/ReflectionDeclare.h>
#include <NsCore/String.h>
#include <NsCore/ReflectionDeclareEnum.h>

class InventoryItemViewModel : public NoesisApp::NotifyPropertyChangedBase {
public:
    InventoryItemViewModel(LocText itemName, int count, TileItemUID uid, StrToken icon);

#define P_ITEM_TEXT "ItemText"
#define P_ITEM_COUNT "ItemCount"
#define P_ITEM_ICON "ItemIcon"

    const char* getText() const;
    void setText(const char* text);

    int getCount() const;
    void setCount(int val);

    const char* getIcon() const;
    void setIcon(const char* icon);
    void setIconFromAsset(StrToken iconAsset);

    NS_DECLARE_REFLECTION(InventoryItemViewModel, NoesisApp::NotifyPropertyChangedBase);

public:
    TileItemUID mUniqueId;
private:
    // Properties
    Noesis::String mText;
    Noesis::String mIcon;
    int mCount;
};
