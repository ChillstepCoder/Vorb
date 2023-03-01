#pragma once

#include "IAgentTask.h"
#include "tile/TileHandle.h"
#include "item/ItemReservation.h"

class ShipItemsForPromiseTask : public IAgentTask
{
public:
    ShipItemsForPromiseTask(ItemPromiseWeakPtr&& itemPromise, TileHandle targetPosition, f32 completionRadius);
    ~ShipItemsForPromiseTask();

    // Override allocation to use boost::singleton_pool
    static void* operator new(size_t count);
    static void operator delete(void* pointer, size_t size);

    bool tick(entt::registry& registry, entt::entity agent) override;

protected:
    ItemPromiseWeakPtr mItemPromise;
    TileHandle mTargetPosition;
    f32 mCompletionRadiusSQ;
};

typedef std::unique_ptr<ShipItemsForPromiseTask> ShipItemsForPromiseTaskPtr;