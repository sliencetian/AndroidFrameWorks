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

#ifndef BERBERIS_BASE_INCLUDE_BERBERIS_BASE_MAPS_SNAPSHOT_H_
#define BERBERIS_BASE_INCLUDE_BERBERIS_BASE_MAPS_SNAPSHOT_H_

#include <cstdint>
#include <mutex>
#include <optional>

#include "berberis/base/arena_alloc.h"
#include "berberis/base/arena_map.h"
#include "berberis/base/arena_string.h"

namespace berberis {

// Stores snapshot mappings from /proc/self/maps for faster access.
// Can contain stale records which is fine for profiling or heuristics,
// but do NOT use it where reliable mapping information is required.
// Call Update() to reread /proc/self/maps.
// Thread-safe, doesn't use malloc.
class MapsSnapshot {
 public:
  static MapsSnapshot* GetInstance();
  void Update();
  // It's important that we return const ArenaString, since arena isn't thread-safe, and we should
  // NOT be triggering re-allocations from outside of this class.
  std::optional<const ArenaString> FindMappedObjectName(uintptr_t addr);
  void ClearForTesting() {
    std::scoped_lock lock(mutex_);
    maps_.clear();
  };

 private:
  MapsSnapshot() : arena_(), mutex_(), maps_(&arena_) {};
  struct Record {
    uintptr_t start;
    uintptr_t end;
    ArenaString pathname;
  };
  Arena arena_;
  std::mutex mutex_;
  ArenaMap<uintptr_t, Record> maps_;
};

}  // namespace berberis

#endif  // BERBERIS_BASE_INCLUDE_BERBERIS_BASE_MAPS_SNAPSHOT_H_
