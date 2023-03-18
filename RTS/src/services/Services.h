#pragma once

// TODO: Can we move this to cpp?
#include <Vorb/ThreadPool.h>

class NavThread;
class ResourceManager;
class ContractManager;

struct ThreadPoolWorkerData {
};

class Services
{
public:

    static void initResources();
    static void initHost();
    static void initCli();
    static void destroy();
    static void destroyResources();

    static void resetThreads();
    static bool isUsingNav() { return sUsingNav; }

    using Threadpool = entt::service_locator<vcore::ThreadPool<ThreadPoolWorkerData>>;
    using NavThread = entt::service_locator<NavThread>;
    using ResourceManager = entt::service_locator<ResourceManager>;
    using ContractManager = entt::service_locator<ContractManager>;
private:
    static void initThreads();
    static bool sUsingNav;
};

