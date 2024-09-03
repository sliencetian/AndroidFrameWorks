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

#include "berberis/runtime_primitives/runtime_library.h"

// TODO: b/352784623 - These need to be implemented by the time we activate
// translation cache.

namespace berberis {

extern "C" {

[[gnu::naked]] [[gnu::noinline]] void berberis_RunGeneratedCode(ThreadState* state, HostCode code) {
  asm("unimp");
}

[[gnu::naked]] [[gnu::noinline]] void berberis_entry_Interpret() {
  asm("unimp");
}

[[gnu::naked]] [[gnu::noinline]] void berberis_entry_ExitGeneratedCode() {
  asm("unimp");
}

[[gnu::naked]] [[gnu::noinline]] void berberis_entry_Stop() {
  asm("unimp");
}

[[gnu::naked]] [[gnu::noinline]] void berberis_entry_NoExec() {
  asm("unimp");
}

[[gnu::naked]] [[gnu::noinline]] void berberis_entry_NotTranslated() {
  asm("unimp");
}

[[gnu::naked]] [[gnu::noinline]] void berberis_entry_Translating() {
  asm("unimp");
}

[[gnu::naked]] [[gnu::noinline]] void berberis_entry_Invalidating() {
  asm("unimp");
}

[[gnu::naked]] [[gnu::noinline]] void berberis_entry_Wrapping() {
  asm("unimp");
}

[[gnu::naked]] [[gnu::noinline]] void berberis_entry_HandleLightCounterThresholdReached() {
  asm("unimp");
}

}  // extern "C"

}  // namespace berberis
