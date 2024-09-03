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

#include "ErasedMessageQueue.hpp"

/*
 * Convert a Rust NativeHandle (passed as its individual fields) to a C++ one.
 * Duplicates the file descriptors, which are passed as integers.
 */
NativeHandle convertHandle(const int* fds, size_t n_fds, const int32_t* ints, size_t n_ints) {
    std::vector<ndk::ScopedFileDescriptor> fdv;
    for (size_t i = 0; i < n_fds; i++) {
        fdv.push_back(ndk::ScopedFileDescriptor(dup(fds[i])));
    }
    std::vector<int32_t> intv(ints, ints + n_ints);

    return NativeHandle(std::move(fdv), intv);
}

GrantorDescriptor convertGrantor(int32_t fdIndex, int32_t offset, int64_t extent) {
    return GrantorDescriptor(fdIndex, offset, extent);
}

ErasedMessageQueueDesc* convertDesc(const GrantorDescriptor* grantors, size_t n_grantors,
                                    const int* handle_fds, size_t handle_n_fds,
                                    const int32_t* handle_ints, size_t handle_n_ints,
                                    int32_t quantum, int32_t flags) {
    std::vector<GrantorDescriptor> grantorsv(grantors, grantors + n_grantors);
    auto&& handle = convertHandle(handle_fds, handle_n_fds, handle_ints, handle_n_ints);

    return new ErasedMessageQueueDesc{
            grantorsv,
            std::move(handle),
            quantum,
            flags,
    };
}

void freeDesc(ErasedMessageQueueDesc* desc) {
    delete desc;
}

const GrantorDescriptor* descGrantors(const ErasedMessageQueueDesc& desc) {
    return desc.grantors.data();
}
size_t descNumGrantors(const ErasedMessageQueueDesc& desc) {
    return desc.grantors.size();
}
const ndk::ScopedFileDescriptor* descHandleFDs(const ErasedMessageQueueDesc& desc) {
    return desc.handle.fds.data();
}
size_t descHandleNumFDs(const ErasedMessageQueueDesc& desc) {
    return desc.handle.fds.size();
}
const int* descHandleInts(const ErasedMessageQueueDesc& desc) {
    return desc.handle.ints.data();
}
size_t descHandleNumInts(const ErasedMessageQueueDesc& desc) {
    return desc.handle.ints.size();
}
int32_t descQuantum(const ErasedMessageQueueDesc& desc) {
    return desc.quantum;
}
int32_t descFlags(const ErasedMessageQueueDesc& desc) {
    return desc.flags;
}

ErasedMessageQueue::ErasedMessageQueue(const ErasedMessageQueueDesc& desc, bool resetPointers)
    : inner(new android::AidlMessageQueue<MQErased, SynchronizedReadWrite>(desc, resetPointers)) {}

ErasedMessageQueue::ErasedMessageQueue(size_t numElementsInQueue, bool configureEventFlagWord,
                                       size_t quantum)
    : inner(new android::AidlMessageQueue<MQErased, SynchronizedReadWrite>(
              numElementsInQueue, configureEventFlagWord, quantum)) {}

bool ErasedMessageQueue::beginWrite(size_t nMessages, MemTransaction* memTx) const {
    MessageQueueBase<AidlMQDescriptorShim, MQErased,
                     FlavorTypeToValue<SynchronizedReadWrite>::value>::MemTransaction memTxInternal;
    auto result = inner->beginWrite(nMessages, &memTxInternal);
    memTx->first = memTxInternal.getFirstRegion();
    memTx->second = memTxInternal.getSecondRegion();
    return result;
};

bool ErasedMessageQueue::commitWrite(size_t nMessages) {
    return inner->commitWrite(nMessages);
}

bool ErasedMessageQueue::beginRead(size_t nMessages, MemTransaction* memTx) const {
    MessageQueueBase<AidlMQDescriptorShim, MQErased,
                     FlavorTypeToValue<SynchronizedReadWrite>::value>::MemTransaction memTxInternal;
    auto result = inner->beginRead(nMessages, &memTxInternal);
    memTx->first = memTxInternal.getFirstRegion();
    memTx->second = memTxInternal.getSecondRegion();
    return result;
}

bool ErasedMessageQueue::commitRead(size_t nMessages) {
    return inner->commitRead(nMessages);
}

ErasedMessageQueueDesc* ErasedMessageQueue::dupeDesc() const {
    return new ErasedMessageQueueDesc(inner->dupeDesc());
}
