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

#include <android-base/logging.h>
#include <android-base/unique_fd.h>
#include <cutils/sockets.h>
#include <sys/un.h>

#include "com_android_aconfig_new_storage.h"
#include "aconfigd.h"
#include "aconfigd_util.h"

using namespace android::aconfigd;
using namespace android::base;

static int aconfigd_platform_init() {
  auto aconfigd = Aconfigd(kAconfigdRootDir,
                           kPersistentStorageRecordsFileName);

  auto init_result = aconfigd.InitializePlatformStorage();
  if (!init_result.ok()) {
    LOG(ERROR) << "failed to initialize platform storage records: " << init_result.error();
    return 1;
  }

  return 0;
}

static int aconfigd_mainline_init() {
  auto aconfigd = Aconfigd(kAconfigdRootDir,
                           kPersistentStorageRecordsFileName);

  auto init_result = aconfigd.InitializeMainlineStorage();
  if (!init_result.ok()) {
    LOG(ERROR) << "failed to initialize mainline storage records: " << init_result.error();
    return 1;
  }

  return 0;
}

/// receive storage requests from socket
static Result<StorageRequestMessages> receiveMessage(int client_fd) {
  unsigned char size_buffer[4] = {};
  int size_bytes_received = 0;
  while (size_bytes_received < 4) {
    auto chunk_bytes =
        TEMP_FAILURE_RETRY(recv(client_fd, size_buffer + size_bytes_received,
                                4 - size_bytes_received, 0));
    if (chunk_bytes <= 0) {
      return ErrnoError() << "received error polling for message size";
    }
    size_bytes_received += chunk_bytes;
  }

  uint32_t payload_size = uint32_t(
      size_buffer[0]<<24 | size_buffer[1]<<16 | size_buffer[2]<<8 | size_buffer[3]);

  char payload_buffer[payload_size];
  int payload_bytes_received = 0;
  while (payload_bytes_received < payload_size) {
    auto chunk_bytes = TEMP_FAILURE_RETRY(
        recv(client_fd, payload_buffer + payload_bytes_received,
             payload_size - payload_bytes_received, 0));
    if (chunk_bytes <= 0) {
      return ErrnoError() << "received error polling for message payload";
    }
    payload_bytes_received += chunk_bytes;
  }

  auto msg = std::string(payload_buffer, payload_bytes_received);

  auto requests = StorageRequestMessages{};
  if (!requests.ParseFromString(msg)) {
      return Error() << "Could not parse message from aconfig storage init socket";
  }
  return requests;
}

/// send return acknowledgement
static Result<void> sendMessage(int client_fd, const StorageReturnMessages& msg) {
  auto content = std::string();
  if (!msg.SerializeToString(&content)) {
    return Error() << "failed to serialize return messages to string";
  }

  unsigned char bytes[4];
  uint32_t msg_size = content.size();
  bytes[0] = (msg_size >> 24) & 0xFF;
  bytes[1] = (msg_size >> 16) & 0xFF;
  bytes[2] = (msg_size >> 8) & 0xFF;
  bytes[3] = (msg_size >> 0) & 0xFF;

  int payload_bytes_sent = 0;
  while (payload_bytes_sent < 4) {
    auto chunk_bytes = TEMP_FAILURE_RETRY(
        send(client_fd, bytes + payload_bytes_sent,
             4 - payload_bytes_sent, 0));
    if (chunk_bytes <= 0) {
      return ErrnoError() << "send() failed for return msg size";
    }
    payload_bytes_sent += chunk_bytes;
  }

  payload_bytes_sent = 0;
  const char* payload_buffer = content.c_str();
  while (payload_bytes_sent < content.size()) {
    auto chunk_bytes = TEMP_FAILURE_RETRY(
        send(client_fd, payload_buffer + payload_bytes_sent,
             content.size() - payload_bytes_sent, 0));
    if (chunk_bytes < 0) {
      return ErrnoError() << "send() failed for return msg";
    }
    payload_bytes_sent += chunk_bytes;
  }

  return {};
}

static int aconfigd_start() {
  auto aconfigd = Aconfigd(kAconfigdRootDir,
                           kPersistentStorageRecordsFileName);

  auto init_result = aconfigd.InitializeInMemoryStorageRecords();
  if (!init_result.ok()) {
    LOG(ERROR) << "Failed to initialize persistent storage records in memory: "
               << init_result.error();
    return 1;
  }

  auto aconfigd_fd = android::base::unique_fd(android_get_control_socket(kAconfigdSocket));
  if (aconfigd_fd == -1) {
    PLOG(ERROR) << "failed to get aconfigd socket";
    return 1;
  }

  if (listen(aconfigd_fd, 8) < 0) {
    PLOG(ERROR) << "failed to listen to socket";
    return 1;
  };

  auto addr = sockaddr_un();
  addr.sun_family = AF_UNIX;
  auto path = std::string("/dev/socket/") + kAconfigdSocket;
  strlcpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path));
  socklen_t addr_len = sizeof(addr);

  while(true) {
    LOG(INFO) << "start accepting client requests";
    auto client_fd = android::base::unique_fd(accept4(
        aconfigd_fd, reinterpret_cast<sockaddr*>(&addr), &addr_len, SOCK_CLOEXEC));
    if (client_fd == -1) {
      PLOG(ERROR) << "failed to establish connection";
      continue;
    }
    LOG(INFO) << "received client requests";

    auto requests = receiveMessage(client_fd.get());
    if (!requests.ok()) {
      LOG(ERROR) << requests.error();
      continue;
    }

    auto return_messages = StorageReturnMessages();
    for (auto& request : requests->msgs()) {
      auto* return_msg = return_messages.add_msgs();
      auto result = aconfigd.HandleSocketRequest(request, *return_msg);
      if (!result.ok()) {
        auto* errmsg = return_msg->mutable_error_message();
        *errmsg = result.error().message();
        LOG(ERROR) << "Failed to handle socket request: " << *errmsg;
      } else {
        LOG(INFO) << "Successfully handled socket request";
      }
    }

    auto result = sendMessage(client_fd.get(), return_messages);
    if (!result.ok()) {
      LOG(ERROR) << result.error();
    }
  }

  return 1;
}

int main(int argc, char** argv) {
  if (!com::android::aconfig_new_storage::enable_aconfig_storage_daemon()) {
    return 0;
  }

  android::base::InitLogging(argv, &android::base::KernelLogger);

  if (argc == 1) {
    return aconfigd_start();
  } else if (argc == 2 && strcmp(argv[1], "--platform_init") == 0) {
    return aconfigd_platform_init();
  } else if (argc == 2 && strcmp(argv[1], "--mainline_init") == 0) {
    return aconfigd_mainline_init();
  } else {
    LOG(ERROR) << "invalid aconfigd command";
    return 1;
  }
}
