#include "stdafx.h"
#include "PhysicsJobSystem.h"

#include "services/Services.h"

PhysicsJobSystem::PhysicsJobSystem(ui32 inMaxBarriers) : JobSystemWithBarrier(inMaxBarriers) {
    constexpr ui32 cMaxJobs = 2048;
    mJobs.Init(cMaxJobs, cMaxJobs);
}

int PhysicsJobSystem::GetMaxConcurrency() const {
    return Services::Threadpool::ref().getNumWorkers() + 1; // +1 for the main thread
}

JPH::JobHandle PhysicsJobSystem::CreateJob(const char* inJobName, JPH::ColorArg inColor, const JPH::JobSystem::JobFunction& inJobFunction, ui32 inNumDependencies /*= 0*/) {
    // Loop until we can get a job from the free list
    ui32 index = mJobs.ConstructObject(inJobName, inColor, this, inJobFunction, inNumDependencies);
    if (index == AvailableJobs::cInvalidObjectIndex) {
        panic("No physics jobs available!");
    }
    Job* job = &mJobs.Get(index);

    // Construct handle to keep a reference, the job is queued below and may immediately complete
    JobHandle handle(job);

    // If there are no dependencies, queue the job now
    if (inNumDependencies == 0)
        QueueJob(job);

    // Return the handle
    return handle;
}

void PhysicsJobSystem::QueueJob(Job* inJob) {
    inJob->AddRef();
    Services::Threadpool::ref().addTask([inJob]() {
        // Execute the job
        inJob->Execute();
        inJob->Release();
    }, TaskPriority::High);
}

void PhysicsJobSystem::QueueJobs(Job** inJobs, ui32 inNumJobs) {
    for (ui32 i = 0; i < inNumJobs; i++) {
        QueueJob(inJobs[i]);
    }
}

void PhysicsJobSystem::FreeJob(Job* inJob) {
    mJobs.DestructObject(inJob);
}
