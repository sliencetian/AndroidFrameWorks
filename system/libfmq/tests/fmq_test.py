#!/usr/bin/env python3
#
# Copyright (C) 2020 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

import subprocess
import unittest

def run_cmd(cmd, ignore_error=False):
    print("Running command:", cmd)
    p = subprocess.Popen(cmd, shell=True)
    p.communicate()
    if not ignore_error and p.returncode:
        raise subprocess.CalledProcessError(p.returncode, cmd)
    return p.returncode

class TestFmq(unittest.TestCase):
    pass

def make_test(client, server, client_filter=None):
    def test(self):
        try:
            run_cmd("adb shell killall %s >/dev/null 2>&1" % client, ignore_error=True)
            run_cmd("adb shell killall %s >/dev/null 2>&1" % server, ignore_error=True)
            run_cmd("adb shell \"( %s ) </dev/null >/dev/null 2>&1 &\"" % server)
            client_cmd = client
            if client_filter:
                client_cmd += " --gtest_filter=" + client_filter
            run_cmd("adb shell %s" % client_cmd)
        finally:
            run_cmd("adb shell killall %s >/dev/null 2>&1" % client, ignore_error=True)
            run_cmd("adb shell killall %s >/dev/null 2>&1" % server, ignore_error=True)
    return test

def has_bitness(bitness):
    return 0 == run_cmd("echo '[[ \"$(getprop ro.product.cpu.abilist%s)\" != \"\" ]]' | adb shell sh" % bitness, ignore_error=True)

if __name__ == '__main__':
    clients = []
    servers = []

    def add_clients_and_servers(clients: list[str], servers: list[str], base: str):
        clients += [
            base + "/fmq_test_client/fmq_test_client",
            base + "/fmq_rust_test_client/fmq_rust_test_client",
        ]
        servers += [base + "/android.hardware.tests.msgq@1.0-service-test/android.hardware.tests.msgq@1.0-service-test"]
        servers += [base + "/android.hardware.tests.msgq@1.0-rust-service-test/android.hardware.tests.msgq@1.0-rust-service-test"]

    if has_bitness(32):
        add_clients_and_servers(clients, servers, "/data/nativetest")

    if has_bitness(64):
        add_clients_and_servers(clients, servers, "/data/nativetest64")

    assert len(clients) > 0
    assert len(servers) > 0

    def bitness(binary_path: str) -> str:
        if "64" in binary_path:
            return "64"
        return "32"

    def short_name(binary_path: str) -> str:
        base = "rust" if "rust" in binary_path else ""
        return base + bitness(binary_path)

    for client in clients:
        for server in servers:
            test_name = 'test_%s_to_%s' % (short_name(client), short_name(server))
            # Tests in the C++ test client that are fully supported by the Rust test server
            rust_tests = ":".join([
                # Only run AIDL tests 0 and 2, not HIDL tests 1 and 3
                "SynchronizedReadWriteClient/0.*",
                "SynchronizedReadWriteClient/2.*",
                # Skip blocking tests until the Rust FMQ interface supports them: TODO(b/339999649)
                "-*Blocking*",
            ])
            # Enable subset of tests if testing C++ client against the rust server
            gtest_filter = rust_tests if "rust" in server and not "rust" in client else None
            test = make_test(client, server, gtest_filter)
            setattr(TestFmq, test_name, test)

    suite = unittest.TestLoader().loadTestsFromTestCase(TestFmq)
    unittest.TextTestRunner(verbosity=2).run(suite)
