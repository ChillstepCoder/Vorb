#pragma once

namespace EditorUtil {
    template <typename T>
    T evaluateCurve(const std::vector<std::pair<f32, T>>& keys, f32 alpha) {
        // TODO: Blend mode
        int leftIndex;
        int rightIndex;

        bool found = false;

        for (int i = 0; i < keys.size(); ++i) {
            if (keys[i].first >= alpha) {
                found = true;
                rightIndex = i;

                // If exact match, set leftIndex to the same as rightIndex
                if (keys[i].first == alpha) {
                    leftIndex = i;
                }
                else if (i > 0) {  // Make sure we don't go out of bounds
                    leftIndex = i - 1;
                }
                else {
                    leftIndex = i;
                }

                break;
            }
        }

        if (!found) {
            rightIndex = keys.size() - 1;
            leftIndex = rightIndex - 1;
            if (leftIndex < 0) leftIndex = 0;
        }

        f32 diff = keys[rightIndex].first - keys[leftIndex].first;
        if (diff == 0.0f) {
            return keys[leftIndex].second;
        }
        else {
            const f32 lerpValue = (alpha - keys[leftIndex].first) / diff;
            return lerp(keys[leftIndex].second, keys[rightIndex].second, lerpValue);
        }
    }

    template <typename T>
    bool updateAndRenderCurve(std::vector<std::pair<f32, T>>& keys, std::function<bool(T&)> controlFunction) {
        bool changed = false;
        ImGui::SameLine();
        if (ImGui::Button("+")) {
            auto&& newKey = keys.emplace_back();
            newKey.first = 1.0f;
            newKey.second = keys[keys.size() - 2].second;
        }
        int id = 33;
        size_t i = 0;
        for (auto&& it = keys.begin(); it != keys.end();) {
            ImGui::PushID(id++);
            auto&& key = *it;
            bool movedTime = ImGui::SliderFloat("Time", &key.first, 0.0f, 1.0f);
            if (movedTime) {
                changed = true;
                // Clamp nearby keys to our value
                for (int j = i - 1; j >= 0; --j) {
                    keys[j].first = glm::min(keys[j].first, key.first);
                }
                for (int j = i + 1; j < (int)keys.size(); ++j) {
                    keys[j].first = glm::max(keys[j].first, key.first);
                }
            }
            if (keys.size() > 1) {
                ImGui::SameLine();
                if (ImGui::Button("-")) {
                    it = keys.erase(it);
                    ImGui::PopID();
                    continue;
                }
            }
            changed |= controlFunction(key.second);
            ImGui::Spacing();
            ++it;
            ++i;
            ImGui::PopID();
        }
        // Sort if needed
        if (changed) {
            std::sort(keys.begin(), keys.end(), [](auto&& a, auto&& b) { return a.first < b.first; });
        }
        return changed;
    }
};