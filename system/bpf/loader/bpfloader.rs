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

//! BPF loader for system and vendor applications

#[cfg(enable_libbpf)]
fn load_libbpf_progs() {
    // Libbpf loader functionality here.
}

#[cfg(not(enable_libbpf))]
fn load_libbpf_progs() {
    // Empty stub for feature flag disabled case
}

fn main() {
    load_libbpf_progs();

    // SAFETY: Linking in the existing legacy bpfloader functionality.
    // Any of the four following bindgen functions can abort() or exit()
    // on failure and execNetBpfLoadDone() execve()'s.
    unsafe {
        bpf_android_bindgen::initLogging();
        bpf_android_bindgen::createBpfFsSubDirectories();
        bpf_android_bindgen::legacyBpfLoader();
        bpf_android_bindgen::execNetBpfLoadDone();
    }
}
