#pragma once

// Jobs are only created on sim thread
class ISimJob {
public:
    ISimJob() = default;
    ISimJob(entt::entity jobOwner) : mJobOwner(jobOwner) {}
    virtual ~ISimJob() = default;

    virtual bool tryAquireNextTaskForSimCharacter(entt::registry& simRegistry, entt::entity simCharacter) = 0;
    virtual bool tryAquireNextTaskForFullCharacter(entt::registry& fullRegistry, entt::entity fullCharacter) = 0;
protected:
    i32 mNumActiveTasks = 0; // A task chain counts as one task
    entt::entity mJobOwner = entt::null;
};