#include "stdafx.h"
#include "AnimMachineDef.h"

KEG_TYPE_DEF_SAME_NAME(AnimMachineDefFileData, kt) {
    kt.addValue("rig", keg::Value::basic(offsetof(AnimMachineDefFileData, mRigName), keg::BasicType::STRING));
    kt.addValue("walk_left", keg::Value::basic(offsetof(AnimMachineDefFileData, mWalkLeftName), keg::BasicType::STRING));
    kt.addValue("walk_right", keg::Value::basic(offsetof(AnimMachineDefFileData, mWalkRightName), keg::BasicType::STRING));
    kt.addValue("walk_front", keg::Value::basic(offsetof(AnimMachineDefFileData, mWalkFrontName), keg::BasicType::STRING));
    kt.addValue("walk_back", keg::Value::basic(offsetof(AnimMachineDefFileData, mWalkBackName), keg::BasicType::STRING));
    kt.addValue("run_left", keg::Value::basic(offsetof(AnimMachineDefFileData, mRunLeftName), keg::BasicType::STRING));
    kt.addValue("run_right", keg::Value::basic(offsetof(AnimMachineDefFileData, mRunRightName), keg::BasicType::STRING));
    kt.addValue("run_front", keg::Value::basic(offsetof(AnimMachineDefFileData, mRunFrontName), keg::BasicType::STRING));
    kt.addValue("run_back", keg::Value::basic(offsetof(AnimMachineDefFileData, mRunBackName), keg::BasicType::STRING));
    kt.addValue("idle", keg::Value::basic(offsetof(AnimMachineDefFileData, mIdleName), keg::BasicType::STRING));
    kt.addValue("idle_combat", keg::Value::basic(offsetof(AnimMachineDefFileData, mIdleCombatName), keg::BasicType::STRING));
}
