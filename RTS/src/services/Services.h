#pragma once

// TODO: Can we move this to cpp?
#include <Vorb/ThreadPool.h>

class NavThread;
class ResourceManager;

struct ThreadPoolWorkerData {
};

class Services
{
public:

    static void init();
    static void destroy();

    static void resetThreads();

    using Threadpool = entt::service_locator<vcore::ThreadPool<ThreadPoolWorkerData>>;
    using NavThread = entt::service_locator<NavThread>;
    using ResourceManager = entt::service_locator<ResourceManager>;
private:
    static void initThreads();
};

