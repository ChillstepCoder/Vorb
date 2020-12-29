#pragma once

#include <Vorb/ui/imgui/imgui.h>

#include "generation/NoiseFunction.hpp"

const double  f64_zero = 0., f64_one = 1., f64_lo_a = -1000000000000000.0, f64_hi_a = +1000000000000000.0;
const double MIN_NOISE_OFFSET = -10000.0;
const double MAX_NOISE_OFFSET = 10000.0;

namespace ImguiView {
    struct Noise {
        static bool view(NoiseFunction& n) {
            bool changed = false;
            ImGui::Text(n.label.c_str());
            changed |= ImGui::SliderInt((n.label + "_octaves").c_str(), &n.octaves, 1, 15);
            changed |= ImGui::SliderScalar((n.label + "_persist").c_str(), ImGuiDataType_Double, &n.persistence, &f64_zero, &f64_one);
            changed |= ImGui::SliderScalar((n.label + "_freq").c_str(), ImGuiDataType_Double, &n.frequency, &f64_zero, &f64_one, "%.10f", ImGuiSliderFlags_Logarithmic);
            changed |= ImGui::SliderScalarN((n.label + "_offset").c_str(), ImGuiDataType_Double, &(n.offset.x), 2, &MIN_NOISE_OFFSET, &MAX_NOISE_OFFSET, "%.10f");
            return changed;
        }
    };
}