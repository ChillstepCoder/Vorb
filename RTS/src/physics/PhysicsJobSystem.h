#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Core/JobSystemWithBarrier.h>
#include <Jolt/Core/FixedSizeFreeList.h>

class PhysicsJobSystem final : public JPH::JobSystemWithBarrier
{
public:
	explicit PhysicsJobSystem(ui32 inMaxBarriers);;

	int GetMaxConcurrency() const override;

	JPH::JobHandle CreateJob(const char* inJobName, JPH::ColorArg inColor, const JPH::JobSystem::JobFunction& inJobFunction, ui32 inNumDependencies = 0) override;

	void QueueJob(Job* inJob) override;

	void QueueJobs(Job** inJobs, ui32 inNumJobs) override;

	void FreeJob(Job* inJob) override;

private:
    /// Array of jobs (fixed size)
    using AvailableJobs = JPH::FixedSizeFreeList<Job>;
    AvailableJobs mJobs;
};

