#pragma once

#include <imgui.h>
#include <imgui_internal.h>

namespace ImguiUtil {
    template <typename T>
    bool ObjectVector(const char* label, std::vector<T>& objects, std::function<bool(T& o, ui32 i)> controlFunc, bool resizable = true, T defaultValue = T()) {
        if (!ImGui::TreeNode(label)) {
            return false;
        }

        bool changed = false;

        if (resizable) {
            if (ImGui::Button("Add")) {
                objects.emplace_back(defaultValue);
                changed = true;
            }
        }
        ImGui::Indent();
        for (ui32 i = 0; i < objects.size(); ++i) {
            ImGui::PushID(i);
            ImGui::SeparatorText(std::to_string(i).c_str());
            bool isDeleted = false;
            if (resizable) {
                if (ImGui::Button("Delete")) {
                    isDeleted = true;
                }
                if (i > 0) {
                    ImGui::SameLine();
                    if (ImGui::Button("^")) {
                        std::swap(objects[i], objects[i - 1]);
                        changed = true;
                    }
                }
                if (i < objects.size() - 1) {
                    ImGui::SameLine();
                    if (ImGui::Button("v")) {
                        std::swap(objects[i], objects[i + 1]);
                        changed = true;
                    }
                }
            }
            changed |= controlFunc(objects[i], i);
            ImGui::PopID();
            if (isDeleted) {
                objects.erase(objects.begin() + i);
                changed = true;
            }
        }
        ImGui::Unindent();
        ImGui::Separator();
        ImGui::TreePop();
        return changed;
    }

};