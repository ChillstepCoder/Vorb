#pragma once

#include "network/NetworkConst.h"

class ReplicationComponent
{
public:
    bool shouldReplicateTo(int clientIndex) {
        return mReplicateTargets & (1 << clientIndex);
    }
    void setReplicateToClient(int clientIndex, bool shouldReplicate) {
        int bitMask = 1 << clientIndex;
        mReplicateTargets = (mReplicateTargets & (~bitMask)) | (bitMask * (int)shouldReplicate);
    }

    ClientBits mReplicateTargets = 0xffff;
    static_assert(sizeof(ClientBits) == 2);
};