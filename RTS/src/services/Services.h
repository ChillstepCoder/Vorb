#pragma once


#include <Vorb/ThreadPool.h>

class PathFinder;
class ResourceManager;

struct ThreadPoolWorkerData {
};

class Services
{
public:

    static void init();
    static void destroy();

    using Threadpool = entt::service_locator<vcore::ThreadPool<ThreadPoolWorkerData>>;
    using PathFinder = entt::service_locator<PathFinder>;
    using ResourceManager = entt::service_locator<ResourceManager>;
};

