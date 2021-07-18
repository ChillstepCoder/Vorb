#include "Vorb/stdafx.h"
#include "Vorb/graphics/BlendState.h"

KEG_ENUM_DEF(BlendStateType, vg::BlendStateType, kt) {
    kt.addValue("alpha", vg::BlendStateType::ALPHA);
    kt.addValue("alpha_premult", vg::BlendStateType::ALPHA_PREMULTIPLIED);
    kt.addValue("add", vg::BlendStateType::ADDITIVE);
    kt.addValue("replace", vg::BlendStateType::REPLACE);
}
static_assert((int)vg::BlendStateType::COUNT == 4, "Init new blend states above");

vg::BlendState::BlendState(GLenum srcFactor, GLenum dstFactor) :
    srcFactor(srcFactor),
    dstFactor(dstFactor)
{

}

void vg::BlendState::set() const
{
    glBlendFunc(srcFactor, dstFactor);
}

void vg::BlendState::set(const BlendStateType state)
{
    vg::sBlendStates.STATE_ARRAY[(int)state].set();
}
