//
// Copyright (C) 2024 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#ifndef BERBERIS_ANDROID_API_LIBVULKAN_SEARCH_H_
#define BERBERIS_ANDROID_API_LIBVULKAN_SEARCH_H_

#include <string.h>

#include <algorithm>
#include <type_traits>

namespace berberis {

// This is essentially std::sorted as in C++20.
template <class ForwardIterator, class Compare>
inline constexpr bool IsSorted(ForwardIterator first, ForwardIterator last, Compare comp) {
  // Advancing past last may be undefined behavior. Handle that separately here.
  if (first == last) {
    return true;
  }
  for (auto tmp = first; ++tmp != last; first = tmp) {
    if (comp(*tmp, *first)) {
      return false;
    }
  }
  return true;
}

inline constexpr bool StrCmpLess(const char* lhs, const char* rhs) {
  if (__builtin_is_constant_evaluated()) {
    for (;;) {
      unsigned char lc = *lhs++;
      unsigned char rc = *rhs++;
      if (lc < rc) {
        return true;
      }
      if (lc > rc) {
        return false;
      }
      if (lc == '\0') {
        return false;
      }
    }
  } else {
    return strcmp(lhs, rhs) < 0;
  }
}

inline constexpr class StrCmpLessName {
 public:
  using is_transparent = void;

  template <typename Type1, typename Type2>
  constexpr bool operator()(const Type1& lhs, const Type2& rhs) {
    return StrCmpLess(lhs.name, rhs.name);
  }
  template <typename Type1>
  constexpr bool operator()(const Type1& lhs, const char* rhs) {
    return StrCmpLess(lhs.name, rhs);
  }
  template <typename Type1>
  constexpr bool operator()(const char* lhs, const Type1& rhs) {
    return StrCmpLess(lhs, rhs.name);
  }
} StrCmpLessName;

template <typename Array>
auto FindElementByName(const Array& array, const char* name) {
  auto element = std::lower_bound(std::begin(array), std::end(array), name, StrCmpLessName);
  if (element != std::end(array) and strcmp(element->name, name) == 0) {
    return element;
  }
  return decltype(element){};
}

}  // namespace berberis

#endif  // BERBERIS_ANDROID_API_LIBVULKAN_SEARCH_H_
