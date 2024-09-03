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

package android.aconfigd.test;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import android.aconfigd.Aconfigd.StorageRequestMessage;
import android.aconfigd.Aconfigd.StorageRequestMessages;
import android.aconfigd.Aconfigd.StorageReturnMessage;
import android.aconfigd.Aconfigd.StorageReturnMessages;
import android.aconfigd.AconfigdClientSocket;
import android.aconfigd.AconfigdFlagInfo;
import android.aconfigd.AconfigdJavaUtils;
import android.util.proto.ProtoOutputStream;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.util.Arrays;
import java.util.List;
import java.util.Map;
import java.util.function.Function;

@RunWith(JUnit4.class)
public class AconfigdJavaUtilsTest {

    @Test
    public void testListFlagsValueInNewStorage_singleFlag() throws Exception {
        String packageName = "android.acondigd.test";
        String flagName = "test_flag";
        String serverValue = "";
        String localValue = "";
        String bootValue = "true";
        String defaultValue = "true";
        boolean hasServerOverride = false;
        boolean isReadWrite = false;
        boolean hasLocalOverride = false;

        AconfigdFlagInfo expectedRev =
                AconfigdFlagInfo.newBuilder()
                        .setBootFlagValue(bootValue)
                        .setDefaultFlagValue(defaultValue)
                        .setFlagName(flagName)
                        .setHasLocalOverride(hasLocalOverride)
                        .setHasServerOverride(hasServerOverride)
                        .setIsReadWrite(isReadWrite)
                        .setLocalFlagValue(localValue)
                        .setPackageName(packageName)
                        .setServerFlagValue(serverValue)
                        .build();

        ProtoOutputStream serverReturn = writeListFlagsRequest(Arrays.asList(expectedRev));

        ByteArrayOutputStream buffer = new ByteArrayOutputStream(serverReturn.getRawSize());
        buffer.write(serverReturn.getBytes(), 0, serverReturn.getRawSize());

        AconfigdClientSocket localSocket =
                new FakeAconfigdClientSocketImpl(
                        input -> new ByteArrayInputStream(buffer.toByteArray()));

        Map<String, AconfigdFlagInfo> flagMap =
                AconfigdJavaUtils.listFlagsValueInNewStorage(localSocket);
        assertTrue(flagMap.containsKey(expectedRev.getFullFlagName()));
        assertEquals(expectedRev, flagMap.get(expectedRev.getFullFlagName()));
    }

    @Test
    public void testListFlagsValueInNewStorage_multiFlags() throws Exception {
        String packageName = "android.acondigd.test";
        String flagName1 = "test_flag1";
        String flagName2 = "test_flag2";
        String serverValue = "";
        String localValue = "";
        String bootValue = "true";
        String defaultValue = "true";
        boolean hasServerOverride = false;
        boolean isReadWrite = false;
        boolean hasLocalOverride = false;

        AconfigdFlagInfo expectedRev1 =
                AconfigdFlagInfo.newBuilder()
                        .setBootFlagValue(bootValue)
                        .setDefaultFlagValue(defaultValue)
                        .setFlagName(flagName1)
                        .setHasLocalOverride(hasLocalOverride)
                        .setHasServerOverride(hasServerOverride)
                        .setIsReadWrite(isReadWrite)
                        .setLocalFlagValue(localValue)
                        .setPackageName(packageName)
                        .setServerFlagValue(serverValue)
                        .build();

        AconfigdFlagInfo expectedRev2 =
                AconfigdFlagInfo.newBuilder()
                        .setBootFlagValue(bootValue)
                        .setDefaultFlagValue(defaultValue)
                        .setFlagName(flagName2)
                        .setHasLocalOverride(hasLocalOverride)
                        .setHasServerOverride(hasServerOverride)
                        .setIsReadWrite(isReadWrite)
                        .setLocalFlagValue(localValue)
                        .setPackageName(packageName)
                        .setServerFlagValue(serverValue)
                        .build();

        ProtoOutputStream serverReturn =
                writeListFlagsRequest(Arrays.asList(expectedRev1, expectedRev2));

        ByteArrayOutputStream buffer = new ByteArrayOutputStream(serverReturn.getRawSize());
        buffer.write(serverReturn.getBytes(), 0, serverReturn.getRawSize());

        AconfigdClientSocket localSocket =
                new FakeAconfigdClientSocketImpl(
                        input -> new ByteArrayInputStream(buffer.toByteArray()));

        Map<String, AconfigdFlagInfo> flagMap =
                AconfigdJavaUtils.listFlagsValueInNewStorage(localSocket);
        assertTrue(flagMap.containsKey(expectedRev1.getFullFlagName()));
        assertTrue(flagMap.containsKey(expectedRev2.getFullFlagName()));
        assertEquals(expectedRev1, flagMap.get(expectedRev1.getFullFlagName()));
        assertEquals(expectedRev2, flagMap.get(expectedRev2.getFullFlagName()));
    }

    @Test
    public void testListFlagsValueInNewStorage_errorMessage() throws Exception {
        ProtoOutputStream serverReturn = new ProtoOutputStream();
        String expectErrorMessage = "invalid";
        long msgsToken = serverReturn.start(StorageReturnMessages.MSGS);
        serverReturn.write(StorageReturnMessage.ERROR_MESSAGE, expectErrorMessage);
        serverReturn.end(msgsToken);

        ByteArrayOutputStream buffer = new ByteArrayOutputStream(serverReturn.getRawSize());
        buffer.write(serverReturn.getBytes(), 0, serverReturn.getRawSize());

        AconfigdClientSocket localSocket =
                new FakeAconfigdClientSocketImpl(
                        input -> new ByteArrayInputStream(buffer.toByteArray()));

        Map<String, AconfigdFlagInfo> flagMap =
                AconfigdJavaUtils.listFlagsValueInNewStorage(localSocket);
        assertTrue(flagMap.isEmpty());
    }

    @Test
    public void testListFlagsValueInNewStorage_checkRequest() throws Exception {

        ProtoOutputStream expectRequests = new ProtoOutputStream();
        long msgsToken = expectRequests.start(StorageRequestMessages.MSGS);
        long msgToken = expectRequests.start(StorageRequestMessage.LIST_STORAGE_MESSAGE);
        expectRequests.write(StorageRequestMessage.ListStorageMessage.ALL, 1);
        expectRequests.end(msgToken);
        expectRequests.end(msgsToken);

        ProtoOutputStream serverReturn = new ProtoOutputStream();
        String expectErrorMessage = "invalid";
        msgsToken = serverReturn.start(StorageReturnMessages.MSGS);
        serverReturn.write(StorageReturnMessage.ERROR_MESSAGE, expectErrorMessage);
        serverReturn.end(msgsToken);

        ByteArrayOutputStream buffer = new ByteArrayOutputStream(serverReturn.getRawSize());
        buffer.write(serverReturn.getBytes(), 0, serverReturn.getRawSize());

        AconfigdClientSocket localSocket =
                new FakeAconfigdClientSocketImpl(
                        input -> {
                            assertArrayEquals(expectRequests.getBytes(), input);
                            return new ByteArrayInputStream(buffer.toByteArray());
                        });

        Map<String, AconfigdFlagInfo> flagMap =
                AconfigdJavaUtils.listFlagsValueInNewStorage(localSocket);
        assertTrue(flagMap.isEmpty());
    }

    private ProtoOutputStream writeListFlagsRequest(List<AconfigdFlagInfo> flags) {
        ProtoOutputStream serverReturn = new ProtoOutputStream();
        long msgsToken = serverReturn.start(StorageReturnMessages.MSGS);
        long listToken = serverReturn.start(StorageReturnMessage.LIST_STORAGE_MESSAGE);

        for (AconfigdFlagInfo flag : flags) {
            long flagToken =
                    serverReturn.start(StorageReturnMessage.ListStorageReturnMessage.FLAGS);
            serverReturn.write(
                    StorageReturnMessage.FlagQueryReturnMessage.PACKAGE_NAME,
                    flag.getPackageName());
            serverReturn.write(
                    StorageReturnMessage.FlagQueryReturnMessage.FLAG_NAME, flag.getFlagName());
            serverReturn.write(
                    StorageReturnMessage.FlagQueryReturnMessage.SERVER_FLAG_VALUE,
                    flag.getServerFlagValue());
            serverReturn.write(
                    StorageReturnMessage.FlagQueryReturnMessage.LOCAL_FLAG_VALUE,
                    flag.getLocalFlagValue());
            serverReturn.write(
                    StorageReturnMessage.FlagQueryReturnMessage.BOOT_FLAG_VALUE,
                    flag.getBootFlagValue());
            serverReturn.write(
                    StorageReturnMessage.FlagQueryReturnMessage.DEFAULT_FLAG_VALUE,
                    flag.getDefaultFlagValue());
            serverReturn.write(
                    StorageReturnMessage.FlagQueryReturnMessage.HAS_SERVER_OVERRIDE,
                    flag.getHasServerOverride());
            serverReturn.write(
                    StorageReturnMessage.FlagQueryReturnMessage.IS_READWRITE,
                    flag.getIsReadWrite());
            serverReturn.write(
                    StorageReturnMessage.FlagQueryReturnMessage.HAS_LOCAL_OVERRIDE,
                    flag.getHasLocalOverride());
            serverReturn.end(flagToken);
        }

        serverReturn.end(listToken);
        serverReturn.end(msgsToken);
        return serverReturn;
    }

    private class FakeAconfigdClientSocketImpl implements AconfigdClientSocket {
        private Function<byte[], InputStream> mSendFunc;

        FakeAconfigdClientSocketImpl(Function<byte[], InputStream> sendFunc) {
            mSendFunc = sendFunc;
        }

        @Override
        public InputStream send(byte[] requests) {
            return mSendFunc.apply(requests);
        }

        @Override
        public void close() {
            return;
        }
    }
}
