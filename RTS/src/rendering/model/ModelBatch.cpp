#include "stdafx.h"
#include "ModelBatch.h"


ModelBatch::~ModelBatch() {
    glDeleteVertexArrays(1, &mVao);
    glDeleteBuffers(1, &mVbo);
    glDeleteBuffers(1, &mIbo);
}

void ModelBatch::bindStaticModelAttribs() const {
    assert(mVao);
    if (mCurrentAttribBinding != AttribBinding::Static) [[unlikely]] {
        unbindCurrentAttribs();
        mCurrentAttribBinding = AttribBinding::Static;
        // Transforms
        glEnableVertexArrayAttrib(mVao, 7);
        glEnableVertexArrayAttrib(mVao, 8);
        glEnableVertexArrayAttrib(mVao, 9);
        glEnableVertexArrayAttrib(mVao, 10);
        glVertexArrayAttribFormat(mVao, 7, 4, GL_FLOAT, GL_FALSE, 0);
        glVertexArrayAttribFormat(mVao, 8, 4, GL_FLOAT, GL_FALSE, sizeof(f32v4));
        glVertexArrayAttribFormat(mVao, 9, 4, GL_FLOAT, GL_FALSE, sizeof(f32v4) * 2);
        glVertexArrayAttribFormat(mVao, 10, 4, GL_FLOAT, GL_FALSE, sizeof(f32v4) * 3);
        glVertexArrayAttribBinding(mVao, 7, MODEL_TRANSFORMS_BINDING_POINT);
        glVertexArrayAttribBinding(mVao, 8, MODEL_TRANSFORMS_BINDING_POINT);
        glVertexArrayAttribBinding(mVao, 9, MODEL_TRANSFORMS_BINDING_POINT);
        glVertexArrayAttribBinding(mVao, 10, MODEL_TRANSFORMS_BINDING_POINT);
        glVertexArrayBindingDivisor(mVao, MODEL_TRANSFORMS_BINDING_POINT, 1);

        // InstanceData
        glEnableVertexArrayAttrib(mVao, 13);
        glVertexArrayAttribIFormat(mVao, 13, 3, GL_UNSIGNED_INT, 0);
        glVertexArrayAttribBinding(mVao, 13, MODEL_INSTANCE_DATA_BINDING_POINT);
        glVertexArrayBindingDivisor(mVao, MODEL_INSTANCE_DATA_BINDING_POINT, 1);
    }
}

void ModelBatch::unbindStaticModelAttribs() const {
    assert(mCurrentAttribBinding == AttribBinding::Static);
    mCurrentAttribBinding = AttribBinding::None;
    glDisableVertexArrayAttrib(mVao, 7);
    glDisableVertexArrayAttrib(mVao, 8);
    glDisableVertexArrayAttrib(mVao, 9);
    glDisableVertexArrayAttrib(mVao, 10);
    glDisableVertexArrayAttrib(mVao, 13);
}

void ModelBatch::bindSkeletalModelAttribs() const {
    assert(mVao);
    if (mCurrentAttribBinding != AttribBinding::Skeletal) [[unlikely]] {
        unbindCurrentAttribs();
        mCurrentAttribBinding = AttribBinding::Skeletal;
        glEnableVertexArrayAttrib(mVao, 11); // Bone weights
        glEnableVertexArrayAttrib(mVao, 12); // Bone ids

        // InstanceData
        glEnableVertexArrayAttrib(mVao, 13);
        glVertexArrayAttribIFormat(mVao, 13, 1, GL_UNSIGNED_INT, 0);
        glVertexArrayAttribBinding(mVao, 13, MODEL_INSTANCE_DATA_BINDING_POINT);
        glVertexArrayBindingDivisor(mVao, MODEL_INSTANCE_DATA_BINDING_POINT, 1);
    }
}

void ModelBatch::unbindSkeletalModelAttribs() const {
    assert(mCurrentAttribBinding == AttribBinding::Skeletal);
    mCurrentAttribBinding = AttribBinding::None;
    glDisableVertexArrayAttrib(mVao, 11); // Bone weights
    glDisableVertexArrayAttrib(mVao, 12); // Bone ids
    glDisableVertexArrayAttrib(mVao, 13); // Instance data
}

void ModelBatch::unbindCurrentAttribs() const {
    switch (mCurrentAttribBinding) {
        case AttribBinding::Static:
            unbindStaticModelAttribs();
            break;
        case AttribBinding::Skeletal:
            unbindSkeletalModelAttribs();
            break;
        default:
            break;
    }
    static_assert(e_count(AttribBinding) == 3, "Update unbindCurrentAttribs");
}