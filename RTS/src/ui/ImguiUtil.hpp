#pragma once

#include <Vorb/ui/imgui/imgui.h>

namespace ImguiUtil {
    inline bool ButtonCenteredOnLine(const char* label, ImVec2 size) {
        ImGuiStyle& style = ImGui::GetStyle();

        float avail = ImGui::GetContentRegionAvail().x;

        float off = (avail - size.x) * 0.5f;
        if (off > 0.0f)
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + off);

        return ImGui::Button(label, size);
    }

}