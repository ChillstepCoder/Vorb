#pragma once

#include <imgui.h>
#include <imgui_internal.h>

namespace ImguiUtil {
    template <typename T>
    bool ObjectVector(const char* label, std::vector<T>& objects, std::function<bool(T& o)> controlFunc) {
        bool changed = false;
        ImGui::Separator();
        ImGui::Text(label);
        if (ImGui::Button("Add")) {
            objects.emplace_back();
        }
        ImGui::Indent();
        for (ui32 i = 0; i < objects.size(); ++i) {
            ImVec2 frameMin = ImGui::GetCursorScreenPos(); // Top left of frame
            ImGui::PushID(i);
            bool isDeleted = false;
            if (ImGui::Button("X")) {
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
            changed |= controlFunc(objects[i]);
            ImGui::PopID();
            if (isDeleted) {
                objects.erase(objects.begin() + i);
                changed = true;
            }
            // Draw a border around the group
            ImGui::GetWindowDrawList()->AddRect(frameMin, ImGui::GetItemRectMax(), IM_COL32(255, 255, 255, 128));
        }
        ImGui::Unindent();
        ImGui::Separator();
        return changed;
    }

};