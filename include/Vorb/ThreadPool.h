//
// ThreadPool.h
// Vorb Engine
//
// Created by Benjamin Arnold on 13 Nov 2014
// Copyright 2014 Regrowth Studios
// MIT License
//

/*! \file ThreadPool.h
 * @brief Provides a general threadpool implementation for distributing work.
 */

#pragma once

#ifndef Vorb_ThreadPool_h__
//! @cond DOXY_SHOW_HEADER_GUARDS
#define Vorb_ThreadPool_h__
//! @endcond

#ifndef VORB_USING_PCH
#include <vector>

#include "Vorb/types.h"
#endif // !VORB_USING_PCH

#include <functional>
#include <thread>
#include <condition_variable>

#include <Vorb/concurrentqueue.h>
#include <Vorb/blockingconcurrentqueue.h>

#include "Vorb/IThreadPoolTask.h"


using ThreadPoolTaskProcs = std::pair<std::function<void()>, std::function<void()>>;

namespace vorb {
    namespace core {

        class ThreadPool {
        public:
            ThreadPool(ui32 size);
            ~ThreadPool();

            void mainThreadUpdate();

            /// Clears all unprocessed tasks from the task queue
            void clearTasks();

            /// Adds a task to the task queue
            /// @param task: The task to add
            /// TODO: Remove mainProc
            void addTask(std::function<void()>&& workerProc, std::function<void()>&& mainProc) {
                mTasks.enqueue(std::make_pair(std::move(workerProc), std::move(mainProc)));
            }

            /// Add an array of tasks to the task queue
            /// @param tasks: The array of tasks to add
            /// @param size: The size of the array
            /*void addTasks(IThreadPoolTask<T>* tasks[], size_t size) {
                mTasks.enqueue_bulk(tasks, size);
            }*/

            /// Getters
            i32 getNumWorkers() const { return mWorkers.size(); }
            size_t getTasksSizeApprox() const { return mTasks.size_approx(); }
            size_t getMainThreadQueuedProcsApprox() const { return mMainThreadProcs.size_approx(); }

            // Adjust number of running threads
            void setSize(ui32 size);
            int getSize() const { return mActiveThreads; }
            int getNumRunningThreads() const { return mRunningThreads; }
        private:
            VORB_NON_COPYABLE(ThreadPool);
            // Typedef for func ptr

            /// Class definition for worker thread
            class WorkerThread {
            public:
                typedef void (ThreadPool::* workerFunc)(WorkerThread*);
                /// Creates the thread
                /// @param func: The function the thread should execute
                WorkerThread(workerFunc func, ThreadPool* threadPool) : thread(func, threadPool, this) {
                }

                ~WorkerThread() {
                    if (thread.joinable()) {
                        thread.join();
                    }
                }
                /// Blocks until the worker thread completes
                void join() {
                    thread.join();
                }

                std::thread thread; ///< The thread handle
                std::atomic_bool mStop;
                std::atomic_bool mActive = true;
            };

            /// Thread function that processes tasks
            /// @param data: The worker specific data
            void workerThreadFunc(WorkerThread* thisThread);

            /// Lock free task queues
            moodycamel::BlockingConcurrentQueue<ThreadPoolTaskProcs> mTasks; ///< Holds tasks to execute
            moodycamel::ConcurrentQueue<std::function<void()>> mMainThreadProcs; ///< Contains functions to run on main thread after complete
           
            std::vector <std::unique_ptr<WorkerThread>> mWorkers; ///< All the worker threads
            std::atomic_int mActiveThreads = 0;
            std::atomic_int mRunningThreads = 0;
        };

    }
}
namespace vcore = vorb::core;

#endif // !Vorb_ThreadPool_h__
