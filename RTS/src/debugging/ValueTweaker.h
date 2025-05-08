#pragma once

void renderTweakerImgui();

void tweakerAdd(const char name[], float* value, float min, float max);

#define TWEAKER_FLOAT(name, startVal, min, max)            \
    static float name = startVal;                          \
    RUNTIME_INIT_FUNC(name ## Func) {                      \
        tweakerAdd(/*__FILE__, */#name, &name, min, max ); \
    }