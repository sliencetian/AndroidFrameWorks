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

#include <sys/socket.h>
#include <sys/un.h>

#include <gtest/gtest.h>
#include <cutils/sockets.h>
#include <android-base/file.h>
#include <android-base/result.h>
#include <android-base/logging.h>
#include <android-base/unique_fd.h>

#include <aconfigd.pb.h>
#include "com_android_aconfig_new_storage.h"

using namespace android::base;

namespace android {
namespace aconfigd {

class AconfigdSocketTest : public ::testing::Test {
 protected:
  Result<unique_fd> connect_aconfigd_socket() {
    auto sock_fd = unique_fd(socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0));
    if (sock_fd == -1) {
      return ErrnoError() << "failed create socket";
    }

    auto addr = sockaddr_un();
    addr.sun_family = AF_UNIX;
    auto path = std::string("/dev/socket/aconfigd");
    strlcpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path));

    bool success = false;
    for (int retry = 0; retry < 5; retry++) {
      if (connect(sock_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0) {
        success = true;
        break;
      }
      sleep(1);
    }

    if (!success) {
      return ErrnoError() << "failed to connect to aconfigd socket";
    }

    return sock_fd;
  }

  // send a message to aconfigd socket, and capture return message
  Result<StorageReturnMessages> send_message(const StorageRequestMessages& messages) {
    auto sock_fd = connect_aconfigd_socket();
    if (!sock_fd.ok()) {
      return Error() << sock_fd.error();
    }

    auto message_string = std::string();
    if (!messages.SerializeToString(&message_string)) {
      return Error() << "failed to serialize pb to string";
    }

    unsigned char bytes[4];
    uint32_t msg_size = message_string.size();
    bytes[0] = (msg_size >> 24) & 0xFF;
    bytes[1] = (msg_size >> 16) & 0xFF;
    bytes[2] = (msg_size >> 8) & 0xFF;
    bytes[3] = (msg_size >> 0) & 0xFF;

    auto num_bytes = TEMP_FAILURE_RETRY(send(*sock_fd, bytes, 4, 0));
    if (num_bytes != 4) {
      return ErrnoError() << "send() failed for msg size";
    }

    num_bytes = TEMP_FAILURE_RETRY(
        send(*sock_fd, message_string.c_str(), message_string.size(), 0));
    if (num_bytes != static_cast<long>(message_string.size())) {
      return ErrnoError() << "send() failed for msg";
    }

    num_bytes = TEMP_FAILURE_RETRY(recv(*sock_fd, bytes, 4, 0));
    if (num_bytes != 4) {
      return ErrnoError() << "recv() failed for return msg size";
    }

    uint32_t payload_size =
        uint32_t(bytes[0]<<24 | bytes[1]<<16 | bytes[2]<<8 | bytes[3]);
    char buffer[payload_size];
    int payload_bytes_received = 0;
    while (payload_bytes_received < payload_size) {
      auto chunk_bytes = TEMP_FAILURE_RETRY(
          recv(*sock_fd, buffer + payload_bytes_received,
               payload_size - payload_bytes_received, 0));
      if (chunk_bytes <= 0) {
        return ErrnoError() << "recv() failed for return msg";
      }
      payload_bytes_received += chunk_bytes;
    }

    auto return_messages = StorageReturnMessages{};
    if (!return_messages.ParseFromString(std::string(buffer, payload_size))) {
      return Error() << "failed to parse string into proto";
    }

    if (return_messages.msgs_size() != messages.msgs_size()) {
      return Error() << "Send " << messages.msgs_size() << " request messages, get "
                     << return_messages.msgs_size() << " return messages";
    }

    return return_messages;
  }

  void add_new_storage_message(StorageRequestMessages& messages) {
    auto* message = messages.add_msgs();
    auto* msg = message->mutable_new_storage_message();
    auto test_dir = base::GetExecutableDirectory();
    msg->set_container("mockup");
    msg->set_package_map(test_dir + "/tests/package.map");
    msg->set_flag_map(test_dir + "/tests/flag.map");
    msg->set_flag_value(test_dir + "/tests/flag.val");
  }

  void add_flag_query_message(StorageRequestMessages& messages,
                              const std::string& package,
                              const std::string& flag) {
    auto* message = messages.add_msgs();
    auto* msg = message->mutable_flag_query_message();
    msg->set_package_name(package);
    msg->set_flag_name(flag);
  }

  void add_list_storage_message(StorageRequestMessages& messages) {
    auto* message = messages.add_msgs();
    auto* msg = message->mutable_list_storage_message();
    msg->set_all(true);
  }

  void verify_new_storage_return_message(const StorageReturnMessage& msg) {
    ASSERT_TRUE(msg.has_new_storage_message()) << msg.error_message();
    auto message = msg.new_storage_message();
    ASSERT_TRUE(message.storage_updated());
  }

  void verify_list_storage_return_message(const StorageReturnMessage& msg) {
    ASSERT_TRUE(msg.has_list_storage_message()) << msg.error_message();
  }

  void verify_error_message(const StorageReturnMessage& msg,
                            const std::string& errmsg) {
    ASSERT_TRUE(msg.has_error_message());
    ASSERT_TRUE(msg.error_message().find(errmsg) != std::string::npos)
        << msg.error_message();
  }
}; // class AconfigdSocketTest

// single request test
TEST_F(AconfigdSocketTest, single_request) {
  if (!com::android::aconfig_new_storage::enable_aconfig_storage_daemon()) {
    return;
  }
  auto request_msgs = StorageRequestMessages();
  add_flag_query_message(request_msgs, "unknown_package", "unknown_flag");
  auto return_msgs = send_message(request_msgs);
  ASSERT_TRUE(return_msgs.ok()) << return_msgs.error();
  verify_error_message(return_msgs->msgs(0), "container not found");
}

// multiple request test
TEST_F(AconfigdSocketTest, multiple_requests) {
  if (!com::android::aconfig_new_storage::enable_aconfig_storage_daemon()) {
    return;
  }
  auto request_msgs = StorageRequestMessages();
  size_t num_msgs = 10;
  for (size_t i=0; i<num_msgs; ++i) {
    add_flag_query_message(request_msgs, "unknown_package", "unknown_flag");
  }
  auto return_msgs = send_message(request_msgs);
  ASSERT_TRUE(return_msgs.ok()) << return_msgs.error();
  for (size_t i=0; i<num_msgs; ++i) {
    verify_error_message(return_msgs->msgs(i), "container not found");
  }
}

// add a mockup container
TEST_F(AconfigdSocketTest, add_new_storage) {
  return;
  auto request_msgs = StorageRequestMessages();
  add_new_storage_message(request_msgs);
  auto return_msgs = send_message(request_msgs);
  ASSERT_TRUE(return_msgs.ok()) << return_msgs.error();
  verify_new_storage_return_message(return_msgs->msgs(0));
}

TEST_F(AconfigdSocketTest, storage_list_package) {
  if (!com::android::aconfig_new_storage::enable_aconfig_storage_daemon()) {
    return;
  }
  auto request_msgs = StorageRequestMessages();
  add_list_storage_message(request_msgs);
  auto return_msgs = send_message(request_msgs);
  ASSERT_TRUE(return_msgs.ok()) << return_msgs.error();
  verify_list_storage_return_message(return_msgs->msgs(0));
}

} // namespace aconfigd
} // namespace android
