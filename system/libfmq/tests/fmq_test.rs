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

//! Crate to wrap tests of libfmq rust bindings with a trivial C-ABI interface
//! to test them from C++.

use fmq::MessageQueue;

macro_rules! assert_return {
    ($e: expr) => {
        if !$e {
            eprintln!(stringify!($e));
            return false;
        }
    };
    ($e: expr, $msg: expr) => {
        if !$e {
            eprintln!($msg);
            return false;
        }
    };
}

fn test_body() -> bool {
    let mut mq = MessageQueue::<u8>::new(500, false);

    match mq.write_many(4) {
        Some(mut wc) => {
            wc.write(200).unwrap();
            wc.write(201).unwrap();
            wc.write(202).unwrap();
            wc.write(203).unwrap();
        }
        None => {
            eprintln!("failed to write_many(4)");
            return false;
        }
    };

    let desc = mq.dupe_desc();
    let join_handle = std::thread::spawn(move || {
        let mut mq2 = MessageQueue::from_desc(&desc, false);
        match mq2.read_many(1) {
            Some(mut rc) => {
                assert_return!(rc.read() == Some(200));
            }
            None => {
                eprintln!("failed to read_many(1)");
                return false;
            }
        };
        true
    });

    assert_return!(join_handle.join().ok() == Some(true));

    match mq.read_many(3) {
        Some(mut rc) => {
            assert_return!(rc.read() == Some(201));
            assert_return!(rc.read() == Some(202));
            assert_return!(rc.read() == Some(203));
            drop(rc);
        }
        None => {
            eprintln!("failed to read_many(4)");
            return false;
        }
    };

    true
}

/// Test fmq from Rust. Returns 0 on failure, 1 on success.
#[no_mangle]
pub extern "C" fn fmq_rust_test() -> u8 {
    test_body() as u8
}
