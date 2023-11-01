#pragma once

#include "util/GlobalEnumNameMap.h"

namespace ImguiUtil {
    // Usage: ImguiUtil::EnumCombo<T>("Label", Val, ENUM_NAME_MAP(T));
    template<typename T>
    inline bool EnumCombo(const char* label, T& val) {
        const auto& nameMap = getGlobalEnumNameMap<T>();
        bool changed = false;
        if (ImGui::BeginCombo(label, nameMap.at(val).data())) {
            for (auto&& it : nameMap) {
                bool isSelected = (val == it.first);
                ImGui::Selectable(it.second.data(), &isSelected);

                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                    if (val != it.first) {
                        val = it.first;
                        changed = true;
                    }
                }
            }
            ImGui::EndCombo();
        }
        return changed;
    }
}