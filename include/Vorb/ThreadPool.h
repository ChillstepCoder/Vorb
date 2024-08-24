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
#include <semaphore>

#include <Vorb/concurrentqueue.h>

#include "Vorb/IThreadPoolTask.h"

enum class TaskPriority {
    High,
    Normal,
    Low,
    COUNT
};

namespace vorb {
    namespace core {

        class ThreadPool {
        public:
            // If we exceed this, we have problems
            static constexpr ui32 MAX_TASKS = 1048576;

            ThreadPool(ui32 size);
            ~ThreadPool();

            /// Clears all unprocessed tasks from the task queue
            void clearTasks();

            /// Adds a task to the task queue
            /// @param task: The task to add
            inline void addTask(std::function<void()> workerProc, TaskPriority priority = TaskPriority::Normal) {
                mTasks[(int)priority].enqueue(workerProc);
                mTaskSemaphore.release();
            }

            /// Getters
            i32 getNumWorkers() const { return mWorkers.size(); }
            size_t getTasksSizeApprox() const { return mTasks[0].size_approx() + mTasks[1].size_approx() + mTasks[2].size_approx(); }
            size_t getTasksSizeApprox(TaskPriority priority) const { return mTasks[(int)priority].size_approx(); }

            // Adjust number of running threads
            void setSize(ui32 size);
            int getSize() const { return mActiveThreads; }
            int getNumRunningThreads() const { return mRunningThreads; }

            bool isRunning() const { return mRunningThreads || getTasksSizeApprox(); }

            bool tryProcessHighPriorityTask();
        private:
            VORB_NON_COPYABLE(ThreadPool);

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
            moodycamel::ConcurrentQueue<std::function<void()>> mTasks[(int)TaskPriority::COUNT]; ///< Holds tasks to execute
            std::counting_semaphore<MAX_TASKS> mTaskSemaphore = std::counting_semaphore<MAX_TASKS>(0);
           
            std::vector<std::unique_ptr<WorkerThread>> mWorkers;
            std::vector<std::unique_ptr<WorkerThread>> mDeadWorkers; // Workers we have released
            std::atomic_int mActiveThreads = 0;
            std::atomic_int mRunningThreads = 0;
        };

    }
}
namespace vcore = vorb::core;

#endif // !Vorb_ThreadPool_h__
