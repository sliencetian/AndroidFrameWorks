//! This module implements the ITestAidlMsgQ AIDL interface

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

use android_fmq_test::aidl::android::fmq::test::ITestAidlMsgQ::ITestAidlMsgQ;
use android_fmq_test::binder::{self, Interface, Result as BinderResult};

/// Struct implementing the ITestAidlMsgQ AIDL interface
#[derive(Default)]
pub struct MsgQTestService {
    queue_sync: std::sync::Mutex<Option<fmq::MessageQueue<i32>>>,
}

impl Interface for MsgQTestService {}

use android_hardware_common_fmq::aidl::android::hardware::common::fmq::{
    MQDescriptor::MQDescriptor, SynchronizedReadWrite::SynchronizedReadWrite,
    UnsynchronizedWrite::UnsynchronizedWrite,
};

impl ITestAidlMsgQ for MsgQTestService {
    /**
     * This method requests the service to set up a synchronous read/write
     * wait-free FMQ using the input descriptor with the client as reader.
     *
     * @param mqDesc This structure describes the FMQ that was set up by the
     * client. Server uses this descriptor to set up a FMQ object at its end.
     *
     * @return True if the setup is successful.
     */
    fn configureFmqSyncReadWrite(
        &self,
        mq_desc: &MQDescriptor<i32, SynchronizedReadWrite>,
    ) -> BinderResult<bool> {
        *self.queue_sync.lock().unwrap() = Some(fmq::MessageQueue::from_desc(mq_desc, false));
        /* TODO(b/339999649) in C++ we set the EventFlag word with bit FMQ_NOT_FULL: */
        /*auto evFlagWordPtr = mFmqSynchronized->getEventFlagWord();
        if (evFlagWordPtr != nullptr) {
            std::atomic_init(evFlagWordPtr, static_cast<uint32_t>(EventFlagBits::FMQ_NOT_FULL));
        }*/

        Ok(true)
    }

    /**
     * This method requests the service to read from the synchronized read/write
     * FMQ.
     *
     * @param count Number to messages to read.
     *
     * @return True if the read operation was successful.
     */
    fn requestReadFmqSync(&self, count: i32) -> BinderResult<bool> {
        let mut queue_guard = self.queue_sync.lock().unwrap();
        let Some(ref mut mq) = *queue_guard else {
            return Err(binder::Status::new_service_specific_error_str(107, Some("no fmq set up")));
        };
        let rc = mq.read_many(count.try_into().unwrap());
        match rc {
            Some(mut rc) => {
                for _ in 0..count {
                    rc.read().unwrap();
                }
                Ok(true)
            }
            None => {
                eprintln!("failed to read_many({count})");
                Ok(false)
            }
        }
    }

    /**
     * This method requests the service to write into the synchronized read/write
     * flavor of the FMQ.
     *
     * @param count Number to messages to write.
     *
     * @return True if the write operation was successful.
     */
    fn requestWriteFmqSync(&self, count: i32) -> BinderResult<bool> {
        let mut queue_guard = self.queue_sync.lock().unwrap();
        let Some(ref mut mq) = *queue_guard else {
            return Err(binder::Status::new_service_specific_error_str(107, Some("no fmq set up")));
        };
        let wc = mq.write_many(count.try_into().unwrap());
        match wc {
            Some(mut wc) => {
                for i in 0..count {
                    wc.write(i).unwrap();
                }
                drop(wc);
                Ok(true)
            }
            None => {
                eprintln!("failed to write_many({count})");
                Ok(false)
            }
        }
    }

    fn getFmqUnsyncWrite(
        &self,
        _: bool,
        _: bool,
        _: &mut MQDescriptor<i32, UnsynchronizedWrite>,
    ) -> BinderResult<bool> {
        // The Rust interface to FMQ does not support `UnsynchronizedWrite`.
        Ok(false)
    }

    /**
     * This method requests the service to trigger a blocking read.
     *
     * @param count Number of messages to read.
     *
     */
    fn requestBlockingRead(&self, _: i32) -> BinderResult<()> {
        todo!("b/339999649")
    }
    fn requestBlockingReadDefaultEventFlagBits(&self, _: i32) -> BinderResult<()> {
        todo!("b/339999649")
    }
    fn requestBlockingReadRepeat(&self, _: i32, _: i32) -> BinderResult<()> {
        todo!("b/339999649")
    }
    fn requestReadFmqUnsync(&self, _: i32) -> BinderResult<bool> {
        // The Rust interface to FMQ does not support `UnsynchronizedWrite`.
        Ok(false)
    }
    fn requestWriteFmqUnsync(&self, _: i32) -> BinderResult<bool> {
        // The Rust interface to FMQ does not support `UnsynchronizedWrite`.
        Ok(false)
    }
}
