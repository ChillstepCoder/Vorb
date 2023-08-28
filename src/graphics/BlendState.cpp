#include "Vorb/stdafx.h"
#include "Vorb/graphics/BlendState.h"

vg::BlendState vg::BlendState::CURR(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_FUNC_ADD);
vg::BlendState vg::BlendState::PREV(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_FUNC_ADD);

vg::BlendStates vg::sBlendStates = {
    {
        vg::BlendState(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_FUNC_ADD), // ALPHA
        vg::BlendState(GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_FUNC_ADD),       // ALPHA_PREMULTIPLIED
        vg::BlendState(GL_SRC_ALPHA, GL_ONE, GL_FUNC_ADD),                 // ADDITIVE
        vg::BlendState(GL_SRC_ALPHA, GL_ONE, GL_FUNC_REVERSE_SUBTRACT),    // SUBTRACTIVE
        vg::BlendState(GL_ONE, GL_ZERO, GL_FUNC_ADD),                      // REPLACE
        vg::BlendState(GL_DST_COLOR, GL_ONE_MINUS_SRC_ALPHA, GL_FUNC_ADD), // MULTIPLY
    }
};
static_assert((int)vg::BlendStateType::COUNT == 6, "Add new blend states above");

KEG_ENUM_DEF(BlendStateType, vg::BlendStateType, kt) {
    kt.addValue("alpha", vg::BlendStateType::ALPHA);
    kt.addValue("alpha_premult", vg::BlendStateType::ALPHA_PREMULTIPLIED);
    kt.addValue("add", vg::BlendStateType::ADDITIVE);
    kt.addValue("subtract", vg::BlendStateType::SUBTRACTIVE);
    kt.addValue("replace", vg::BlendStateType::REPLACE);
    kt.addValue("multiply", vg::BlendStateType::MULTIPLY);
}
static_assert((int)vg::BlendStateType::COUNT == 6, "Init new blend states above");

vg::BlendState::BlendState(GLenum srcFactor, GLenum dstFactor, GLint blendEquation) :
    srcFactor(srcFactor),
    dstFactor(dstFactor),
    blendEquation(blendEquation)
{

}

void vg::BlendState::set() const {
    PREV = CURR;
    CURR = *this;
    glBlendFunc(srcFactor, dstFactor);
    glBlendEquation(blendEquation);
}

void vg::BlendState::set(const BlendStateType state)
{
    vg::sBlendStates.STATE_ARRAY[(int)state].set();
}

void vorb::graphics::BlendState::restorePrevious() {
    CURR = PREV;
    CURR.set();
}
