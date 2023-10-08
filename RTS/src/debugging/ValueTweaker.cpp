#include "stdafx.h"
#include "ValueTweaker.h"

#include <Vorb/ui/imgui/imgui.h>
#include <Vorb/ui/imgui/backends/imgui_impl_sdl2.h>
#include <Vorb/ui/imgui/backends/imgui_impl_opengl3.h>

enum class TweakerEntryType {
    FLOAT,
    COUNT
};

struct FloatTweakerEntry {
    f32* value;
    f32 min;
    f32 max;
};

struct TweakerEntry {
    union {
        FloatTweakerEntry floatEntry;
    };
    TweakerEntryType type;
    nString label;
};
static_assert(e_cast(TweakerEntryType::COUNT) == 1, "Add new entry type");

struct Tweaker {
    std::vector<TweakerEntry> entries;
};

Tweaker sTweaker;

void renderTweakerImgui() {
    for (auto& it : sTweaker.entries) {
        switch (it.type) {
            case TweakerEntryType::FLOAT: {
                ImGui::SliderFloat(it.label.c_str(), it.floatEntry.value, it.floatEntry.min, it.floatEntry.max);
                break;
            }
            default:
                assert(false);
        }
    }
}

void tweakerAdd(const char name[], float* value, float min, float max)
{
    TweakerEntry newEntry;
    newEntry.type = TweakerEntryType::FLOAT;
    newEntry.floatEntry.value = value;
    newEntry.floatEntry.min = min;
    newEntry.floatEntry.max = max;
    newEntry.label = name;
    sTweaker.entries.push_back(newEntry);
}
