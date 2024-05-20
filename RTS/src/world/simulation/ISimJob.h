#pragma once

// Jobs are only created on sim thread
class ISimJob {
public:
    ISimJob() = default;
    ISimJob(entt::entity jobOwner) : mJobOwner(jobOwner) {}
    virtual ~ISimJob() = default;

    virtual bool tryAquireNextTaskForSimCharacter(entt::registry& simRegistry, entt::entity simCharacter) = 0;
    virtual bool tryAquireNextTaskForFullCharacter(entt::registry& fullRegistry, entt::entity fullCharacter) = 0;

    void addTaskHandle(SimTaskHandle* handle) { mTaskHandles.emplace_back(handle); }
    void removeTaskHandle(SimTaskHandle* handle) {
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
    i32 getRefCount() const { return mTaskHandles.size(); }
protected:
    i32 mNumActiveTasks = 0; // A task chain counts as one task
    entt::entity mJobOwner = entt::null;
    std::vector<SimTaskHandle*> mTaskHandles; // Task handles held by characters who are using us
};
