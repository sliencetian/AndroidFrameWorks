//! Test service implementation for FMQ.

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

use android_fmq_test::aidl::android::fmq::test::ITestAidlMsgQ::BnTestAidlMsgQ;

use fmq_test_service_rust_impl::MsgQTestService;

use android_fmq_test::binder;

fn main() {
    binder::ProcessState::start_thread_pool();

    let service: MsgQTestService = Default::default();

    const SERVICE_IDENTIFIER: &str = "android.fmq.test.ITestAidlMsgQ/default";
    log::info!("instance: {SERVICE_IDENTIFIER}");

    // Register AIDL service
    let test_service_binder =
        BnTestAidlMsgQ::new_binder(service, binder::BinderFeatures::default());
    binder::add_service(SERVICE_IDENTIFIER, test_service_binder.as_binder())
        .expect("Failed to register service");
    binder::ProcessState::join_thread_pool();

    std::process::exit(1); // Should not be reached
}
