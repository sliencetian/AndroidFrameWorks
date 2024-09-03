/*
 * Copyright (C) 2024 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <android-base/thread_annotations.h>
#include <any>
#include <functional>
#include <mutex>
#include <vector>

namespace android::audio_utils {

/**
 * The DeferredExecutor class accumulates objects to dispose
 * and functors to execute.
 *
 * The class is used in a worker thread loop to allow objects
 * and functors to be accumulated under a mutex,
 * where such object dtors or functors might cause
 * deadlocks or order inversion issues when executed.
 * The process() method is then called outside of the mutex
 * to dispose any objects and execute any functors accumulated.
 */

class DeferredExecutor {
public:

    /**
     * \param processInDtor if true calls process in the dtor.
     *
     * processInDtor defaults to false to prevent use after free.
     */
    explicit DeferredExecutor(bool processInDtor = false)
        : mProcessInDtor(processInDtor)
    {}

    /**
     * If processInDtor is set true in the ctor, then
     * deferred functors are executed.  Then any
     * deferred functors and garbage are deallocated.
     */
    ~DeferredExecutor() {
        if (mProcessInDtor) process(true /* recursive */);
    }

    /**
     * Delays destruction of an object to the
     * invocation of process() (generally outside of lock).
     *
     * Example Usage:
     *
     * std::vector<std::vector<sp<IFoo>>> interfaces;
     * ...
     * executor.dispose(std::move(interfaces));
     */
    template<typename T>
    void dispose(T&& object) {
        std::lock_guard lg(mMutex);
        mGarbage.emplace_back(std::forward<T>(object));
    }

    /**
     * Defers execution of a functor to the invocation
     * of process() (generally outside of lock).
     *
     * Example Usage:
     *
     * executor.defer([]{ foo(); });
     */
    template<typename F>
    void defer(F&& functor) {
        std::lock_guard lg(mMutex);
        mDeferred.emplace_back(std::forward<F>(functor));
    }

    /**
     * Runs deferred functors (in order of adding)
     * and then dellocates the functors and empties the garbage
     * (in reverse order of adding).
     *
     * \param recursive if set to true, will loop the process
     *     to ensure no garbage or deferred objects remain.
     */
    void process(bool recursive = false) {
        do {
            // Note the declaration order of garbage and deferred.
            std::vector <std::any> garbage;
            std::vector <std::function<void()>> deferred;
            {
                std::lock_guard lg(mMutex);
                if (mGarbage.empty() && mDeferred.empty()) return;
                std::swap(garbage, mGarbage);
                std::swap(deferred, mDeferred);
            }
            // execution in order of adding.
            // destruction in reverse order of adding.
            for (const auto& f: deferred) {
                f();
            }
        } while (recursive);
    }

    /**
     * Skips running any deferred functors and dellocates the functors
     * and empties the garbage (in reverse order of adding).
     */
    void clear() {
        // Note the declaration order of garbage and deferred.
        std::vector<std::any> garbage;
        std::vector<std::function<void()>> deferred;
        {
            std::lock_guard lg(mMutex);
            std::swap(garbage, mGarbage);
            std::swap(deferred, mDeferred);
        }
    }

    /**
     * Returns true if there is no garbage and no deferred methods.
     */
    bool empty() const {
        std::lock_guard lg(mMutex);
        return mGarbage.empty() && mDeferred.empty();
    }

private:
    const bool mProcessInDtor;
    mutable std::mutex mMutex;
    std::vector<std::any> mGarbage GUARDED_BY(mMutex);
    std::vector<std::function<void()>> mDeferred GUARDED_BY(mMutex);
};

}  // namespace android::audio_utils
