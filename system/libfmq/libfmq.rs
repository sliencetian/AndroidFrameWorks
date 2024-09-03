//! libfmq Rust wrapper

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

use fmq_bindgen::{
    convertDesc, convertGrantor, descFlags, descGrantors, descHandleFDs, descHandleInts,
    descHandleNumFDs, descHandleNumInts, descNumGrantors, descQuantum, freeDesc,
    ndk_ScopedFileDescriptor, ErasedMessageQueue, ErasedMessageQueueDesc, GrantorDescriptor,
    MQDescriptor, MemTransaction, NativeHandle, ParcelFileDescriptor, SynchronizedReadWrite,
};

use std::ptr::addr_of_mut;

use log::error;

/// A trait indicating that a type is safe to pass through shared memory.
///
/// # Safety
///
/// This requires that the type must not contain any capabilities such as file
/// descriptors or heap allocations, and that it must be permitted to access
/// all bytes of its representation (so it must not contain any padding bytes).
///
/// Because being stored in shared memory the allows the type to be accessed
/// from different processes, it may also be accessed from different threads in
/// the same process. As such, `Share` is a supertrait of `Sync`.
pub unsafe trait Share: Sync {}

// SAFETY: All types implementing the `zerocopy::AsBytes` trait implement `Share`.
unsafe impl<T: zerocopy::AsBytes + zerocopy::FromBytes + Send + Sync> Share for T {}

/// An IPC message queue for values of type T.
pub struct MessageQueue<T> {
    inner: ErasedMessageQueue,
    ty: core::marker::PhantomData<T>,
}

/** A write completion from the MessageQueue::write() method.

This completion mutably borrows the MessageQueue to prevent concurrent writes;
these must be forbidden because the underlying AidlMessageQueue only stores the
number of outstanding writes, not which have and have not completed, so they
must complete in order. */
#[must_use]
pub struct WriteCompletion<'a, T: Share> {
    inner: MemTransaction,
    queue: &'a mut MessageQueue<T>,
    n_elems: usize,
    n_written: usize,
}

impl<'a, T: Share> WriteCompletion<'a, T> {
    /// Obtain a pointer to the location at which the idx'th item should be
    /// stored.
    ///
    /// The returned pointer is only valid while `self` has not been dropped and
    /// is invalidated by any call to `self.write`. The pointer should be used
    /// with `std::ptr::write` or a DMA API to initialize the underlying storage
    /// before calling `assume_written` to indicate how many elements were
    /// written.
    ///
    /// It is only permitted to access at most `contiguous_count(idx)` items
    /// via offsets from the returned address.
    ///
    /// Calling this method with a greater `idx` may return a pointer to another
    /// memory region of different size than the first.
    pub fn ptr(&self, idx: usize) -> *mut T {
        if idx >= self.n_elems {
            panic!(
                "indexing out of bound: ReadCompletion for {} elements but idx {} accessed",
                self.n_elems, idx
            )
        }
        ptr(&self.inner, idx)
    }

    /// Return the number of contiguous elements that may be stored starting at
    /// the given index in the backing buffer corresponding to the given index.
    ///
    /// Intended for use with the `ptr` method.
    ///
    /// Returns 0 if `idx` is greater than or equal to the completion's element
    /// count.
    pub fn contiguous_count(&self, idx: usize) -> usize {
        contiguous_count(&self.inner, idx, self.n_elems)
    }

    /// Returns how many elements still must be written to this WriteCompletion
    /// before dropping it.
    pub fn required_elements(&self) -> usize {
        assert!(self.n_written <= self.n_elems);
        self.n_elems - self.n_written
    }

    /// Write one item to `self`. Fails and returns the item if `self` is full.
    pub fn write(&mut self, data: T) -> Result<(), T> {
        if self.required_elements() > 0 {
            // SAFETY: `self.ptr(self.n_written)` is known to be uninitialized.
            // The dtor of data, if any, will not run because `data` is moved
            // out of here.
            unsafe { self.ptr(self.n_written).write(data) };
            self.n_written += 1;
            Ok(())
        } else {
            Err(data)
        }
    }

    /// Promise to the `WriteCompletion` that `n_newly_written` elements have
    /// been written with unsafe code or DMA to the pointer returned by the
    /// `ptr` method.
    ///
    /// Panics if `n_newly_written` exceeds the number of elements yet required.
    ///
    /// # Safety
    /// It is UB to call this method except after calling the `ptr` method and
    /// writing the specified number of values of type T to that location.
    pub unsafe fn assume_written(&mut self, n_newly_written: usize) {
        assert!(n_newly_written < self.required_elements());
        self.n_written += n_newly_written;
    }
}

impl<'a, T: Share> Drop for WriteCompletion<'a, T> {
    fn drop(&mut self) {
        if self.n_written < self.n_elems {
            error!(
                "WriteCompletion dropped without writing to all elements ({}/{} written)",
                self.n_written, self.n_elems
            );
        }
        let txn = std::mem::take(&mut self.inner);
        self.queue.commit_write(txn);
    }
}

impl<T: Share> MessageQueue<T> {
    const fn type_size() -> usize {
        std::mem::size_of::<T>()
    }

    /// Create a new MessageQueue with capacity for `elems` elements.
    pub fn new(elems: usize, event_word: bool) -> Self {
        Self {
            // SAFETY: Calling bindgen'd constructor. The only argument that
            // can't be validated by the implementation is the quantum, which
            // must equal the element size.
            inner: unsafe { ErasedMessageQueue::new1(elems, event_word, Self::type_size()) },
            ty: core::marker::PhantomData,
        }
    }

    /// Create a MessageQueue connected to another existing instance from its
    /// descriptor.
    pub fn from_desc(desc: &MQDescriptor<T, SynchronizedReadWrite>, reset_pointers: bool) -> Self {
        let mut grantors = desc
            .grantors
            .iter()
            // SAFETY: this just forwards the integers to the GrantorDescriptor
            // constructor; GrantorDescriptor is POD.
            .map(|g| unsafe { convertGrantor(g.fdIndex, g.offset, g.extent) })
            .collect::<Vec<_>>();

        // SAFETY: These pointer/length pairs come from Vecs that will outlive
        // this function call, and the call itself copies all data it needs out
        // of them.
        let cpp_desc = unsafe {
            convertDesc(
                grantors.as_mut_ptr(),
                grantors.len(),
                desc.handle.fds.as_ptr().cast(),
                desc.handle.fds.len(),
                desc.handle.ints.as_ptr(),
                desc.handle.ints.len(),
                desc.quantum,
                desc.flags,
            )
        };
        // SAFETY: Calling bindgen'd constructor which does not store cpp_desc,
        // but just passes it to the initializer of AidlMQDescriptorShim, which
        // deep-copies it.
        let inner = unsafe { ErasedMessageQueue::new(cpp_desc, reset_pointers) };
        // SAFETY: we must free the desc returned by convertDesc; the pointer
        // was just returned above so we know it is valid.
        unsafe { freeDesc(cpp_desc) };
        Self { inner, ty: core::marker::PhantomData }
    }

    /// Obtain a copy of the MessageQueue's descriptor, which may be used to
    /// access it remotely.
    pub fn dupe_desc(&self) -> MQDescriptor<T, SynchronizedReadWrite> {
        // SAFETY: dupeDesc may be called on any valid ErasedMessageQueue; it
        // simply forwards to dupeDesc on the inner AidlMessageQueue and wraps
        // in a heap allocation.
        let erased_desc: *mut ErasedMessageQueueDesc = unsafe { self.inner.dupeDesc() };
        let grantor_to_rust =
            |g: &fmq_bindgen::aidl_android_hardware_common_fmq_GrantorDescriptor| {
                GrantorDescriptor { fdIndex: g.fdIndex, offset: g.offset, extent: g.extent }
            };

        let scoped_to_parcel_fd = |fd: &ndk_ScopedFileDescriptor| {
            use std::os::fd::{BorrowedFd, FromRawFd, OwnedFd};
            // SAFETY: the fd is already open as an invariant of ndk::ScopedFileDescriptor, so
            // it will not be -1, as required by BorrowedFd.
            let borrowed = unsafe { BorrowedFd::borrow_raw(fd._base as i32) };
            ParcelFileDescriptor::new(match borrowed.try_clone_to_owned() {
                Ok(fd) => fd,
                Err(e) => {
                    error!("could not dup NativeHandle fd {}: {}", fd._base, e);
                    // SAFETY: OwnedFd requires the fd is not -1. If we failed to dup the fd,
                    // other code downstream will fail, but we can do no better than pass it on.
                    unsafe { OwnedFd::from_raw_fd(fd._base as i32) }
                }
            })
        };

        // First, we create a desc with the wrong type, because we cannot create one whole cloth of
        // our desired return type unless T implements Default. This Default requirement is
        // superfluous (T::default() is never called), so we then transmute to our desired type.
        let desc = MQDescriptor::<(), SynchronizedReadWrite>::default();
        // SAFETY: This transmute changes only the element type parameter of the MQDescriptor. The
        // layout of an MQDescriptor does not depend on T as T appears in it only in PhantomData.
        let mut desc: MQDescriptor<T, SynchronizedReadWrite> = unsafe { std::mem::transmute(desc) };
        // SAFETY: These slices are created out of the pointer and length pairs exposed by the
        // individual descFoo accessors, so we know they are valid pointer/lengths and point to
        // data that will continue to exist as long as the desc does.
        //
        // Calls to the descFoo accessors on erased_desc are sound because we know inner.dupeDesc
        // returns a valid pointer to a new heap-allocated ErasedMessageQueueDesc.
        let (grantors, fds, ints, quantum, flags) = unsafe {
            let grantors = slice_from_raw_parts_or_empty(
                descGrantors(erased_desc),
                descNumGrantors(erased_desc),
            );
            let fds = slice_from_raw_parts_or_empty(
                descHandleFDs(erased_desc),
                descHandleNumFDs(erased_desc),
            );
            let ints = slice_from_raw_parts_or_empty(
                descHandleInts(erased_desc),
                descHandleNumInts(erased_desc),
            );
            let quantum = descQuantum(erased_desc);
            let flags = descFlags(erased_desc);
            (grantors, fds, ints, quantum, flags)
        };
        let fds = fds.iter().map(scoped_to_parcel_fd).collect();
        let ints = ints.to_vec();
        desc.grantors = grantors.iter().map(grantor_to_rust).collect();
        desc.handle = NativeHandle { fds, ints };
        desc.quantum = quantum;
        desc.flags = flags;
        // SAFETY: we must free the desc returned by dupeDesc; the pointer was
        // just returned above so we know it is valid.
        unsafe { freeDesc(erased_desc) };
        desc
    }

    /// Begin a write transaction. The returned WriteCompletion can be used to
    /// write into the region allocated for the transaction.
    pub fn write(&mut self) -> Option<WriteCompletion<T>> {
        self.write_many(1)
    }

    /// Begin a write transaction for multiple items. See `MessageQueue::write`.
    pub fn write_many(&mut self, n: usize) -> Option<WriteCompletion<T>> {
        let txn = self.begin_write(n)?;
        Some(WriteCompletion { inner: txn, queue: self, n_elems: n, n_written: 0 })
    }

    fn commit_write(&mut self, txn: MemTransaction) -> bool {
        // SAFETY: simply calls commitWrite with the txn length. The txn must
        // only use its first MemRegion.
        unsafe { self.inner.commitWrite(txn.first.length + txn.second.length) }
    }

    fn begin_write(&self, n: usize) -> Option<MemTransaction> {
        let mut txn: MemTransaction = Default::default();
        // SAFETY: we pass a raw pointer to txn, which is used only during the
        // call to beginWrite to write the txn's MemRegion fields, which are raw
        // pointers and lengths pointing into the queue. The pointer to txn is
        // not stored.
        unsafe { self.inner.beginWrite(n, addr_of_mut!(txn)) }.then_some(txn)
    }
}

/// Forms a slice from a pointer and a length.
///
/// Returns an empty slice when `data` is a null pointer and `len` is zero.
///
/// # Safety
///
/// This function has the same safety requirements as [`std::slice::from_raw_parts`],
/// but unlike that function, does not exhibit undefined behavior when `data` is a
/// null pointer and `len` is zero. In this case, it returns an empty slice.
unsafe fn slice_from_raw_parts_or_empty<'a, T>(data: *const T, len: usize) -> &'a [T] {
    if data.is_null() && len == 0 {
        &[]
    } else {
        // SAFETY: The caller must guarantee to satisfy the safety requirements
        // of the standard library function [`std::slice::from_raw_parts`].
        unsafe { std::slice::from_raw_parts(data, len) }
    }
}

#[inline(always)]
fn ptr<T: Share>(txn: &MemTransaction, idx: usize) -> *mut T {
    let (base, region_idx) = if idx < txn.first.length {
        (txn.first.address, idx)
    } else {
        (txn.second.address, idx - txn.first.length)
    };
    let byte_count: usize = region_idx.checked_mul(MessageQueue::<T>::type_size()).unwrap();
    base.wrapping_byte_offset(byte_count.try_into().unwrap()) as *mut T
}

#[inline(always)]
fn contiguous_count(txn: &MemTransaction, idx: usize, n_elems: usize) -> usize {
    if idx > n_elems {
        return 0;
    }
    let region_len = if idx < txn.first.length { txn.first.length } else { txn.second.length };
    region_len - idx
}

/** A read completion from the MessageQueue::read() method.

This completion mutably borrows the MessageQueue to prevent concurrent reads;
these must be forbidden because the underlying AidlMessageQueue only stores the
number of outstanding reads, not which have and have not completed, so they
must complete in order. */
#[must_use]
pub struct ReadCompletion<'a, T: Share> {
    inner: MemTransaction,
    queue: &'a mut MessageQueue<T>,
    n_elems: usize,
    n_read: usize,
}

impl<'a, T: Share> ReadCompletion<'a, T> {
    /// Obtain a pointer to the location at which the idx'th item is located.
    ///
    /// The returned pointer is only valid while `self` has not been dropped and
    /// is invalidated by any call to `self.read`. The pointer should be used
    /// with `std::ptr::read` or a DMA API before calling `assume_read` to
    /// indicate how many elements were read.
    ///
    /// It is only permitted to access at most `contiguous_count(idx)` items
    /// via offsets from the returned address.
    ///
    /// Calling this method with a greater `idx` may return a pointer to another
    /// memory region of different size than the first.
    pub fn ptr(&self, idx: usize) -> *mut T {
        if idx >= self.n_elems {
            panic!(
                "indexing out of bound: ReadCompletion for {} elements but idx {} accessed",
                self.n_elems, idx
            )
        }
        ptr(&self.inner, idx)
    }

    /// Return the number of contiguous elements located starting at the given
    /// index in the backing buffer corresponding to the given index.
    ///
    /// Intended for use with the `ptr` method.
    ///
    /// Returns 0 if `idx` is greater than or equal to the completion's element
    /// count.
    pub fn contiguous_count(&self, idx: usize) -> usize {
        contiguous_count(&self.inner, idx, self.n_elems)
    }

    /// Returns how many elements still must be read from `self` before dropping
    /// it.
    pub fn unread_elements(&self) -> usize {
        assert!(self.n_read <= self.n_elems);
        self.n_elems - self.n_read
    }

    /// Read one item from the `self`. Fails and returns `()` if `self` is empty.
    pub fn read(&mut self) -> Option<T> {
        if self.unread_elements() > 0 {
            // SAFETY: `self.ptr(self.n_read)`is known to be filled with a valid
            // instance of type `T`.
            let data = unsafe { self.ptr(self.n_read).read() };
            self.n_read += 1;
            Some(data)
        } else {
            None
        }
    }

    /// Promise to the `ReadCompletion` that `n_newly_read` elements have
    /// been read with unsafe code or DMA from the pointer returned by the
    /// `ptr` method.
    ///
    /// Panics if `n_newly_read` exceeds the number of elements still unread.
    ///
    /// Calling this method without actually reading the elements will result
    /// in them being leaked without destructors (if any) running.
    pub fn assume_read(&mut self, n_newly_read: usize) {
        assert!(n_newly_read < self.unread_elements());
        self.n_read += n_newly_read;
    }
}

impl<'a, T: Share> Drop for ReadCompletion<'a, T> {
    fn drop(&mut self) {
        if self.n_read < self.n_elems {
            error!(
                "ReadCompletion dropped without reading all elements ({}/{} read)",
                self.n_read, self.n_elems
            );
        }
        let txn = std::mem::take(&mut self.inner);
        self.queue.commit_read(txn);
    }
}

impl<T: Share> MessageQueue<T> {
    /// Begin a read transaction. The returned `ReadCompletion` can be used to
    /// write into the region allocated for the transaction.
    pub fn read(&mut self) -> Option<ReadCompletion<T>> {
        self.read_many(1)
    }

    /// Begin a read transaction for multiple items. See `MessageQueue::read`.
    pub fn read_many(&mut self, n: usize) -> Option<ReadCompletion<T>> {
        let txn = self.begin_read(n)?;
        Some(ReadCompletion { inner: txn, queue: self, n_elems: n, n_read: 0 })
    }

    fn commit_read(&mut self, txn: MemTransaction) -> bool {
        // SAFETY: simply calls commitRead with the txn length. The txn must
        // only use its first MemRegion.
        unsafe { self.inner.commitRead(txn.first.length + txn.second.length) }
    }

    fn begin_read(&self, n: usize) -> Option<MemTransaction> {
        let mut txn: MemTransaction = Default::default();
        // SAFETY: we pass a raw pointer to txn, which is used only during the
        // call to beginRead to write the txn's MemRegion fields, which are raw
        // pointers and lengths pointing into the queue. The pointer to txn is
        // not stored.
        unsafe { self.inner.beginRead(n, addr_of_mut!(txn)) }.then_some(txn)
    }
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn slice_from_raw_parts_or_empty_with_nonempty() {
        const SLICE: &[u8] = &[1, 2, 3, 4, 5, 6];
        // SAFETY: We are constructing a slice from the pointer and length of valid slice.
        let from_raw_parts = unsafe {
            let ptr = SLICE.as_ptr();
            let len = SLICE.len();
            slice_from_raw_parts_or_empty(ptr, len)
        };
        assert_eq!(SLICE, from_raw_parts);
    }

    #[test]
    fn slice_from_raw_parts_or_empty_with_null_pointer_zero_length() {
        // SAFETY: Calling `slice_from_raw_parts_or_empty` with a null pointer
        // and a zero length is explicitly allowed by its safety requirements.
        // In this case, `std::slice::from_raw_parts` has undefined behavior.
        let empty_from_raw_parts = unsafe {
            let ptr: *const u8 = std::ptr::null();
            let len = 0;
            slice_from_raw_parts_or_empty(ptr, len)
        };
        assert_eq!(&[] as &[u8], empty_from_raw_parts);
    }
}
