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

#include <fmq/AidlMessageQueue.h>

using aidl::android::hardware::common::NativeHandle;
using aidl::android::hardware::common::fmq::GrantorDescriptor;
using aidl::android::hardware::common::fmq::SynchronizedReadWrite;

using namespace android;

struct MemTransaction {
    MessageQueueBase<AidlMQDescriptorShim, MQErased,
                     FlavorTypeToValue<SynchronizedReadWrite>::value>::MemRegion first;
    MessageQueueBase<AidlMQDescriptorShim, MQErased,
                     FlavorTypeToValue<SynchronizedReadWrite>::value>::MemRegion second;
};

typedef MQDescriptor<MQErased, SynchronizedReadWrite> ErasedMessageQueueDesc;

GrantorDescriptor convertGrantor(int32_t fdIndex, int32_t offset, int64_t extent);

/**
 * Construct a C++ AIDL MQDescriptor<MQErased, SynchronizedReadWrite> (aka an
 * ErasedMessageQueueDesc) from the fields of a Rust AIDL
 * MQDescriptor<MQErased, SynchronizedReadWrite>.
 *
 * These two types are semantically equivalent but come from separate AIDL
 * codegen backends, so we must convert between them. To convert in the opposite
 * direction, use the descFoo methods to access each field, and manually build
 * the Rust AIDL MQDescriptor<MQErased, SynchronizedReadWrite> instance;
 * see the Rust MessageQueue<T>::dupe_desc method.
 *
 * @param grantors Pointer to the start of the MQDescriptor's GrantorDescriptor
 * array.
 * @param n_grantors The length of the MQDescriptor's GrantorDescriptor array.
 * @param handle_fds Pointer to the start of the MQDescriptor's NativeHandle's
 * file-descriptor array. Ownership of array and contents is not transferred.
 * @param handle_n_fds The corresponding length.
 * @param handle_ints Pointer to the start of the MQDescriptor's NativeHandle's
 * integer array. Ownership of the array is not transferred.
 * @param handle_n_ints The corresponding length.
 * @param quantum The MQDescriptor's quantum.
 * @param flags The MQDescriptor's flags.
 *
 * @return A heap-allocated ErasedMessageQueueDesc instance owned by the caller,
 * which must be freed with freeDesc.
 */
ErasedMessageQueueDesc* convertDesc(const GrantorDescriptor* grantors, size_t n_grantors,
                                    const int* handle_fds, size_t handle_n_fds,
                                    const int32_t* handle_ints, size_t handle_n_ints,
                                    int32_t quantum, int32_t flags);

/**
 * Free a heap-allocated ErasedMessageQueueDesc. Simply calls delete.
 *
 * @param desc The ErasedMessageQueueDesc to free.
 */
void freeDesc(ErasedMessageQueueDesc* desc);

/**
 * The following functions project out individual fields of an
 * ErasedMessageQueueDesc as FFI-safe types to enable constructing a Rust AIDL
 * MQDescriptor<MQErased, SynchronizedReadWrite> from a C++ AIDL one. See the
 * Rust MessageQueue<T>::dupe_desc method.
 */

const GrantorDescriptor* descGrantors(const ErasedMessageQueueDesc& desc);
size_t descNumGrantors(const ErasedMessageQueueDesc& desc);
const ndk::ScopedFileDescriptor* descHandleFDs(const ErasedMessageQueueDesc& desc);
size_t descHandleNumFDs(const ErasedMessageQueueDesc& desc);
const int* descHandleInts(const ErasedMessageQueueDesc& desc);
size_t descHandleNumInts(const ErasedMessageQueueDesc& desc);
int32_t descQuantum(const ErasedMessageQueueDesc& desc);
int32_t descFlags(const ErasedMessageQueueDesc& desc);

/**
 * ErasedMessageQueue is a monomorphized wrapper around AidlMessageQueue that lets
 * us wrap it in an idiomatic Rust API. It does not statically know its element
 * type, but treats elements as opaque objects whose size is given by the
 * MQDescriptor.
 */
class ErasedMessageQueue {
    /* This must be a unique_ptr because bindgen cannot handle by-value fields
     * of template class type. */
    std::unique_ptr<AidlMessageQueue<MQErased, SynchronizedReadWrite>> inner;

  public:
    ErasedMessageQueue(const ErasedMessageQueueDesc& desc, bool resetPointers = true);
    ErasedMessageQueue(size_t numElementsInQueue, bool configureEventFlagWord, size_t quantum);

    /**
     * Get a MemTransaction object to write `nMessages` elements.
     * Once the write is performed using the information from MemTransaction,
     * the write operation is to be committed using a call to commitWrite().
     *
     * @param nMessages Number of messages of the element type.
     * @param Pointer to MemTransaction struct that describes memory to write
     * `nMessages` items of the element type. If a write of size `nMessages` is
     * not possible, the base addresses in the `MemTransaction` object will be
     * set to nullptr.
     *
     * @return Whether it is possible to write `nMessages` items into the FMQ.
     */
    bool beginWrite(size_t nMessages, MemTransaction* memTx) const;

    /**
     * Commit a write of size `nMessages`. To be only used after a call to
     * `beginWrite()`.
     *
     * @param nMessages number of messages of the element type to be written.
     *
     * @return Whether the write operation of size `nMessages` succeeded.
     */
    bool commitWrite(size_t nMessages);

    /**
     * Get a MemTransaction object to read `nMessages` elements.
     * Once the read is performed using the information from MemTransaction,
     * the read operation is to be committed using a call to `commitRead()`.
     *
     * @param nMessages Number of messages of the element type.
     * @param pointer to MemTransaction struct that describes memory to read
     * `nMessages` items of the element type. If a read of size `nMessages` is
     * not possible, the base pointers in the `MemTransaction` object returned
     * will be set to nullptr.
     *
     * @return bool Whether it is possible to read `nMessages` items from the
     * FMQ.
     */
    bool beginRead(size_t nMessages, MemTransaction* memTx) const;

    /**
     * Commit a read of size `nMessages`. To be only used after a call to
     * `beginRead()`.
     *
     * @param nMessages number of messages of the element type to be read.
     *
     * @return bool Whether the read operation of size `nMessages` succeeded.
     */
    bool commitRead(size_t nMessages);

    /**
     * Create a copy of the MQDescriptor for this object. This descriptor can be
     * sent over IPC to allow constructing a remote object that will access the
     * same queue over shared memory.
     *
     * @return ErasedMessageQueueDesc The copied descriptor, which must be freed
     * by passing it to freeDesc.
     */
    ErasedMessageQueueDesc* dupeDesc() const;
};
