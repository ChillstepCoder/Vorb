#pragma once

#include "ai/tasks/IAgentTask.h"

enum EmployeeComponentFlags {
    FLAG_EMPLOYEE_IS_IDLE = 1 << 0
};

struct EmployeeComponent {
    entt::entity mBusiness;
    ui8 flags = 0;
    IAgentTaskPtr mCurrentTask = nullptr;
};

