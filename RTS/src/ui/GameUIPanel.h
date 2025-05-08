#pragma once

#include <NsCore/ReflectionImplementEnum.h>

enum class GameUIPanel {
    Inventory,
    SackContainer,
    COUNT
};
NS_IMPLEMENT_INLINE_REFLECTION_ENUM(GameUIPanel, "GameUIPanel") {
    NsVal("Inventory", GameUIPanel::Inventory);
    NsVal("SackContainer", GameUIPanel::SackContainer);
}