#include "stdafx.h"
#include "MaterialUtils.h"

#include "options/DebugOptions.h"
#include "Material.h"

void MaterialUtils::uploadLightingUniforms(const Material& material) {
    glUniform1f(material.getUniform("unGamma"), sDebugOptions.mGamma);
    glUniform1f(material.getUniform("unExposure"), sDebugOptions.mExposure);
    glUniform1i(material.getUniform("unTonemapOperator"), sDebugOptions.mToneMapOperator);
    glUniform1i(material.getUniform("unLightingModel"), sDebugOptions.mLightingModel);
    glUniform1f(material.getUniform("unHazeExponent"), sDebugOptions.mHazeExponent);
}
