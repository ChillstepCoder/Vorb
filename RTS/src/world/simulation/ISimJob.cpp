#include "stdafx.h"
#include "ISimJob.h"

void ISimJob::removeTaskHandle(SimTaskHandle* handle)
{
    bool found = false;
    for (size_t i = 0; i < mTaskHandles.size(); ++i) {
        if (mTaskHandles[i] == handle) {
            found = true;
            mTaskHandles[i] = mTaskHandles.back();
            mTaskHandles.pop_back();
            break;
        }
    }
    assert(found);
    if (mTaskHandles.empty()) mTaskHandles.shrink_to_fit();
}

void ISimJob::onFinishedInternal() {
    assert(false);
}
