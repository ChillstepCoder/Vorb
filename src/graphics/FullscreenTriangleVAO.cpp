#include "Vorb/stdafx.h"
#include "Vorb/graphics/FullscreenTriangleVAO.h"

#ifndef VORB_USING_PCH
#include <GL/glew.h>
#endif // !VORB_USING_PCH

vg::FullscreenTriangleVAO sGlobalFullTriangleVAO;

void vg::FullscreenTriangleVAO::init() {
    if (!m_vao) {
        glCreateVertexArrays(1, &m_vao);
    }
}

void vg::FullscreenTriangleVAO::dispose() {
    if (m_vao) {
        glDeleteVertexArrays(1, &m_vao);
        m_vao = 0;
    }
}

void vg::FullscreenTriangleVAO::draw() const{
    assert(m_vao);
    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}
