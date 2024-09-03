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

//! Crate containing protos used in aconfigd
// When building with the Android tool-chain
//
//   - an external crate `aconfig_protos` will be generated
//   - the feature "cargo" will be disabled
//
// When building with cargo
//
//   - a local sub-module will be generated in OUT_DIR and included in this file
//   - the feature "cargo" will be enabled
//
// This module hides these differences from the rest of aconfig.

// ---- When building with the Android tool-chain ----
#[cfg(not(feature = "cargo"))]
mod auto_generated {
    pub use aconfigd_rust_proto::aconfigd::storage_request_message::list_storage_message::Msg as ProtoListStorageMessageMsg;
    pub use aconfigd_rust_proto::aconfigd::storage_request_message::ListStorageMessage as ProtoListStorageMessage;
    pub use aconfigd_rust_proto::aconfigd::storage_request_message::Msg as ProtoStorageRequestMessageMsg;
    pub use aconfigd_rust_proto::aconfigd::storage_return_message::FlagQueryReturnMessage as ProtoFlagQueryReturnMessage;
    pub use aconfigd_rust_proto::aconfigd::storage_return_message::ListStorageReturnMessage as ProtoListStorageReturnMessage;
    pub use aconfigd_rust_proto::aconfigd::storage_return_message::Msg as ProtoStorageReturnMessageMsg;
    pub use aconfigd_rust_proto::aconfigd::LocalFlagOverrides as ProtoLocalFlagOverrides;
    pub use aconfigd_rust_proto::aconfigd::StorageRequestMessage as ProtoStorageRequestMessage;
    pub use aconfigd_rust_proto::aconfigd::StorageRequestMessages as ProtoStorageRequestMessages;
    pub use aconfigd_rust_proto::aconfigd::StorageReturnMessage as ProtoStorageReturnMessage;
    pub use aconfigd_rust_proto::aconfigd::StorageReturnMessages as ProtoStorageReturnMessages;
}

// ---- When building with cargo ----
#[cfg(feature = "cargo")]
mod auto_generated {
    // include! statements should be avoided (because they import file contents verbatim), but
    // because this is only used during local development, and only if using cargo instead of the
    // Android tool-chain, we allow it
    include!(concat!(env!("OUT_DIR"), "/aconfigd_proto/mod.rs"));
    pub use aconfigd::storage_request_message::list_storage_message::Msg as ProtoListStorageMessageMsg;
    pub use aconfigd::storage_request_message::ListStorageMessage as ProtoListStorageMessage;
    pub use aconfigd::storage_request_message::Msg as ProtoStorageRequestMessageMsg;
    pub use aconfigd::storage_return_message::FlagQueryReturnMessage as ProtoFlagQueryReturnMessage;
    pub use aconfigd::storage_return_message::ListStorageReturnMessage as ProtoListStorageReturnMessage;
    pub use aconfigd::storage_return_message::Msg as ProtoStorageReturnMessageMsg;
    pub use aconfigd::LocalFlagOverrides as ProtoLocalFlagOverrides;
    pub use aconfigd::StorageRequestMessage as ProtoStorageRequestMessage;
    pub use aconfigd::StorageRequestMessages as ProtoStorageRequestMessages;
    pub use aconfigd::StorageReturnMessage as ProtoStorageReturnMessage;
    pub use aconfigd::StorageReturnMessages as ProtoStorageReturnMessages;
}

pub use auto_generated::*;
