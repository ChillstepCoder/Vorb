#pragma once

// TODO: Can we move this to cpp?
#include <Vorb/ThreadPool.h>

class NavThread;
class ResourceManager;
class ContractManager;
class TimestepManager;

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

    using Threadpool = entt::service_locator<vcore::ThreadPool>;
    using NavThread = entt::service_locator<NavThread>;
    using ResourceManager = entt::service_locator<ResourceManager>;
    using ContractManager = entt::service_locator<ContractManager>;
    using TimestepManager = entt::service_locator<TimestepManager>;
private:
    static void initThreads();
    static bool sUsingNav;
};

