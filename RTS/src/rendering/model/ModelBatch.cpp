#include "stdafx.h"
#include "ModelBatch.h"


ModelBatch::~ModelBatch() {
    glDeleteVertexArrays(1, &mVao);
    glDeleteBuffers(1, &mVbo);
    glDeleteBuffers(1, &mIbo);
}
