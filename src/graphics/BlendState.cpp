#include "Vorb/stdafx.h"
#include "Vorb/graphics/BlendState.h"

vg::BlendState vg::BlendState::CURR(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
vg::BlendState vg::BlendState::PREV(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

KEG_ENUM_DEF(BlendStateType, vg::BlendStateType, kt) {
    kt.addValue("alpha", vg::BlendStateType::ALPHA);
    kt.addValue("alpha_premult", vg::BlendStateType::ALPHA_PREMULTIPLIED);
    kt.addValue("add", vg::BlendStateType::ADDITIVE);
    kt.addValue("replace", vg::BlendStateType::REPLACE);
    kt.addValue("multiply", vg::BlendStateType::MULTIPLY);
}
static_assert((int)vg::BlendStateType::COUNT == 5, "Init new blend states above");

vg::BlendState::BlendState(GLenum srcFactor, GLenum dstFactor) :
    srcFactor(srcFactor),
    dstFactor(dstFactor)
{

}

void vg::BlendState::set() const
{
    PREV = CURR;
    CURR = *this;
    glBlendFunc(srcFactor, dstFactor);
}

void vg::BlendState::set(const BlendStateType state)
{
    vg::sBlendStates.STATE_ARRAY[(int)state].set();
}

void vorb::graphics::BlendState::restorePrevious() {
    CURR = PREV;
    CURR.set();
}
