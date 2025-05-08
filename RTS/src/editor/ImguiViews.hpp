#pragma once

#include <imgui.h>

#include "generation/NoiseFunction.hpp"

const double  f64_zero = 0., f64_one = 1., f64_negone = -1., f64_onethousand = 1000., f64_lo_a = -1000000000000000.0, f64_hi_a = +1000000000000000.0;
const double MIN_NOISE_OFFSET = -10000.0;
const double MAX_NOISE_OFFSET = 10000.0;

namespace ImguiView {
    struct Noise {
        static bool view(NoiseFunction& n, ui32& imguiID) {
            bool changed = false;
            if (ImGui::CollapsingHeader(n.label.toString().c_str())) {
                ImGui::PushID(++imguiID);
                changed |= ImGui::SliderInt("octaves", &n.octaves, 1, 15);
                changed |= ImGui::SliderScalar("persistence", ImGuiDataType_Double, &n.persistence, &f64_zero, &f64_one);
                changed |= ImGui::SliderScalar("frequency", ImGuiDataType_Double, &n.frequency, &f64_zero, &f64_one, "%.10f", ImGuiSliderFlags_Logarithmic);
                changed |= ImGui::SliderScalarN("pos offset", ImGuiDataType_Double, &(n.posOffset.x), 2, &MIN_NOISE_OFFSET, &MAX_NOISE_OFFSET, "%.10f");
                changed |= ImGui::SliderScalar("amplitude", ImGuiDataType_Double, &n.amplitude, &f64_zero, &f64_onethousand, "%.10f", ImGuiSliderFlags_Logarithmic);
                changed |= ImGui::SliderScalar("height offset", ImGuiDataType_Double, &n.heightOffset, &f64_negone, &f64_one, "%.10f");
                ImGui::PopID();
            }
            return changed;
        }
    };
}