#pragma once


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

    using Threadpool = entt::service_locator<vcore::ThreadPool<ThreadPoolWorkerData>>;
    using NavThread = entt::service_locator<NavThread>;
    using ResourceManager = entt::service_locator<ResourceManager>;
};

