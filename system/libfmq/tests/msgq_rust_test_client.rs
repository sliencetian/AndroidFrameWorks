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
use binder::Strong;
use fmq::MessageQueue;

fn wait_get_test_service() -> Result<Strong<dyn ITestAidlMsgQ>, String> {
    const SERVICE_IDENTIFIER: &str = "android.fmq.test.ITestAidlMsgQ/default";
    let service = binder::get_interface::<dyn ITestAidlMsgQ>(SERVICE_IDENTIFIER)
        .map_err(|e| format!("Failed to connect to service {SERVICE_IDENTIFIER}: {e}"))?;
    Ok(service)
}

fn setup_test_service() -> (MessageQueue<i32>, Strong<dyn ITestAidlMsgQ>) {
    let service = wait_get_test_service().expect("failed to obtain test service");

    /* SAFETY: `sysconf` simply returns an integer. */
    let page_size: usize = unsafe { libc::sysconf(libc::_SC_PAGESIZE) }.try_into().unwrap();
    let num_elements_in_sync_queue: usize = (page_size - 16) / std::mem::size_of::<i32>();

    /* Create a queue on the client side. */
    let mq = MessageQueue::<i32>::new(
        num_elements_in_sync_queue,
        true, /* configure event flag word */
    );
    let desc = mq.dupe_desc();

    let result = service.configureFmqSyncReadWrite(&desc);
    assert!(result.is_ok(), "configuring event queue failed");

    (mq, service)
}

mod synchronized_read_write_client {
    use super::*;

    /*
     * Write a small number of messages to FMQ. Request
     * mService to read and verify that the write was successful.
     */
    #[test]
    fn small_input_writer_test() {
        let (mut mq, service) = setup_test_service();
        const DATA_LEN: usize = 16;

        let data: [i32; DATA_LEN] = init_data();
        let mut wc = mq.write_many(DATA_LEN).expect("write_many(DATA_LEN) failed");
        for x in data {
            wc.write(x).expect("writing i32 failed");
        }
        drop(wc);
        let ret = service.requestReadFmqSync(DATA_LEN as _);
        assert!(ret.is_ok());
    }
}

fn init_data<const N: usize>() -> [i32; N] {
    let mut data = [0; N];
    for (i, elem) in data.iter_mut().enumerate() {
        *elem = i as _;
    }
    data
}
