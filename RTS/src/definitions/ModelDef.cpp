#include "stdafx.h"
#include "ModelDef.h"

KEG_TYPE_DEF_SAME_NAME(ModelDefFileData, kt) {
    kt.addValue("model", keg::Value::basic(offsetof(ModelDefFileData, mModelName), keg::BasicType::STRING));
    kt.addValue("rig", keg::Value::basic(offsetof(ModelDefFileData, mRigName), keg::BasicType::STRING));
    kt.addValue("machine", keg::Value::basic(offsetof(ModelDefFileData, mMachineName), keg::BasicType::STRING));
    kt.addValue("scale", keg::Value::basic(offsetof(ModelDefFileData, mScale), keg::BasicType::F32));
    kt.addValue("shadow_detail", keg::Value::custom(offsetof(ModelDefFileData, mShadowDetail), "ShadowLodDetail", true));
    kt.addValue("force_normals_up", keg::Value::basic(offsetof(ModelDefFileData, mForceNormalsUp), keg::BasicType::BOOL));
}
