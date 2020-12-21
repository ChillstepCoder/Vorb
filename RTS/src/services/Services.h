#pragma once

#include <Vorb/ThreadPool.h>


struct ThreadPoolWorkerData {
};

class Services
{
public:

    static void init();
    static void destroy();

    using Threadpool = entt::service_locator<vcore::ThreadPool<ThreadPoolWorkerData>>;
};

