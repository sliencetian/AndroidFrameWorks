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

package android.aconfigd;

import android.aconfigd.Aconfigd.StorageRequestMessage;
import android.aconfigd.Aconfigd.StorageRequestMessages;
import android.aconfigd.Aconfigd.StorageReturnMessage;
import android.aconfigd.Aconfigd.StorageReturnMessages;
import android.util.Slog;
import android.util.proto.ProtoInputStream;
import android.util.proto.ProtoOutputStream;

import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayDeque;
import java.util.Deque;
import java.util.HashMap;
import java.util.Map;

/** @hide */
public class AconfigdJavaUtils {

    private static String TAG = "AconfigdJavaUtils";

    public static AconfigdClientSocket getAconfigdClientSocket() {
        return new AconfigdClientSocketImpl();
    }

    /**
     * serialize a storage reset request proto via proto output stream
     *
     * @param proto
     * @hide
     */
    public static void writeResetStorageRequest(ProtoOutputStream proto) {
        long msgsToken = proto.start(StorageRequestMessages.MSGS);
        long msgToken = proto.start(StorageRequestMessage.RESET_STORAGE_MESSAGE);
        proto.write(StorageRequestMessage.ResetStorageMessage.ALL, true);
        proto.end(msgToken);
        proto.end(msgsToken);
    }

    /**
     * deserialize a flag input proto stream and log
     *
     * @param proto
     * @hide
     */
    public static void writeFlagOverrideRequest(
            ProtoOutputStream proto,
            String packageName,
            String flagName,
            String flagValue,
            boolean isLocal) {
        long msgsToken = proto.start(StorageRequestMessages.MSGS);
        long msgToken = proto.start(StorageRequestMessage.FLAG_OVERRIDE_MESSAGE);
        proto.write(StorageRequestMessage.FlagOverrideMessage.PACKAGE_NAME, packageName);
        proto.write(StorageRequestMessage.FlagOverrideMessage.FLAG_NAME, flagName);
        proto.write(StorageRequestMessage.FlagOverrideMessage.FLAG_VALUE, flagValue);
        proto.write(StorageRequestMessage.FlagOverrideMessage.IS_LOCAL, isLocal);
        proto.end(msgToken);
        proto.end(msgsToken);
    }

    /**
     * deserialize a flag input proto stream and log
     *
     * @param inputStream
     * @hide
     */
    public static void parseAndLogAconfigdReturn(InputStream inputStream) throws IOException {
        ProtoInputStream proto = new ProtoInputStream(inputStream);
        while (true) {
            switch (proto.nextField()) {
                case (int) StorageReturnMessages.MSGS:
                    long msgsToken = proto.start(StorageReturnMessages.MSGS);
                    switch (proto.nextField()) {
                        case (int) StorageReturnMessage.FLAG_OVERRIDE_MESSAGE:
                            Slog.i(TAG, "successfully handled override requests");
                            long msgToken = proto.start(StorageReturnMessage.FLAG_OVERRIDE_MESSAGE);
                            proto.end(msgToken);
                            break;
                        case (int) StorageReturnMessage.ERROR_MESSAGE:
                            String errmsg = proto.readString(StorageReturnMessage.ERROR_MESSAGE);
                            Slog.i(TAG, "override request failed: " + errmsg);
                            break;
                        case ProtoInputStream.NO_MORE_FIELDS:
                            break;
                        default:
                            Slog.e(
                                    TAG,
                                    "invalid message type, expecting only flag override return or"
                                            + " error message");
                            break;
                    }
                    proto.end(msgsToken);
                    break;
                case ProtoInputStream.NO_MORE_FIELDS:
                    return;
                default:
                    Slog.e(TAG, "invalid message type, expect storage return message");
                    break;
            }
        }
    }

    /**
     * this method will new flag value into new storage, and stage the new values
     *
     * @param propsToStage the map of flags <namespace, <flagName, value>>
     * @param isLocal indicates whether this is a local override
     * @hide
     */
    public static void stageFlagsInNewStorage(
            AconfigdClientSocket localSocket,
            Map<String, Map<String, String>> propsToStage,
            boolean isLocal) {
        // write aconfigd requests proto to proto output stream
        int num_requests = 0;
        ProtoOutputStream requests = new ProtoOutputStream();
        for (Map.Entry<String, Map<String, String>> entry : propsToStage.entrySet()) {
            String actualNamespace = entry.getKey();
            Map<String, String> flagValuesToStage = entry.getValue();
            for (String fullFlagName : flagValuesToStage.keySet()) {
                String stagedValue = flagValuesToStage.get(fullFlagName);
                int idx = fullFlagName.lastIndexOf(".");
                if (idx == -1) {
                    Slog.i(TAG, "invalid flag name: " + fullFlagName);
                    continue;
                }
                String packageName = fullFlagName.substring(0, idx);
                String flagName = fullFlagName.substring(idx + 1);
                writeFlagOverrideRequest(requests, packageName, flagName, stagedValue, isLocal);
                ++num_requests;
            }
        }

        if (num_requests == 0) {
            return;
        }

        // send requests to aconfigd and obtain the return
        InputStream returns = localSocket.send(requests.getBytes());

        // deserialize back using proto input stream
        try {
            parseAndLogAconfigdReturn(returns);
        } catch (IOException ioe) {
            Slog.e(TAG, "failed to parse aconfigd return", ioe);
        }
    }

    /** @hide */
    public static Map<String, AconfigdFlagInfo> listFlagsValueInNewStorage(
            AconfigdClientSocket localSocket) {

        ProtoOutputStream requests = new ProtoOutputStream();
        long msgsToken = requests.start(StorageRequestMessages.MSGS);
        long msgToken = requests.start(StorageRequestMessage.LIST_STORAGE_MESSAGE);
        requests.write(StorageRequestMessage.ListStorageMessage.ALL, 1);
        requests.end(msgToken);
        requests.end(msgsToken);

        InputStream inputStream = localSocket.send(requests.getBytes());
        ProtoInputStream res = new ProtoInputStream(inputStream);
        Map<String, AconfigdFlagInfo> flagMap = new HashMap<>();
        Deque<Long> tokens = new ArrayDeque<>();
        try {
            while (res.nextField() != ProtoInputStream.NO_MORE_FIELDS) {
                switch (res.getFieldNumber()) {
                    case (int) StorageReturnMessages.MSGS:
                        tokens.push(res.start(StorageReturnMessages.MSGS));
                        break;
                    case (int) StorageReturnMessage.LIST_STORAGE_MESSAGE:
                        tokens.push(res.start(StorageReturnMessage.LIST_STORAGE_MESSAGE));
                        while (res.nextField() != ProtoInputStream.NO_MORE_FIELDS) {
                            switch (res.getFieldNumber()) {
                                case (int) StorageReturnMessage.ListStorageReturnMessage.FLAGS:
                                    tokens.push(
                                            res.start(
                                                    StorageReturnMessage.ListStorageReturnMessage
                                                            .FLAGS));
                                    AconfigdFlagInfo flagQueryReturnMessage = readFromProto(res);
                                    flagMap.put(
                                            flagQueryReturnMessage.getFullFlagName(),
                                            flagQueryReturnMessage);
                                    res.end(tokens.pop());
                                    break;
                                default:
                                    Slog.w(
                                            TAG,
                                            "Could not read undefined field: "
                                                    + res.getFieldNumber());
                            }
                        }
                        break;
                    case (int) StorageReturnMessage.ERROR_MESSAGE:
                        String errmsg = res.readString(StorageReturnMessage.ERROR_MESSAGE);
                        Slog.w(TAG, "list request failed: " + errmsg);
                        break;
                    default:
                        Slog.w(TAG, "Could not read undefined field: " + res.getFieldNumber());
                }
            }
        } catch (IOException e) {
            Slog.e(TAG, "Failed to read protobuf input stream.", e);
        }

        while (!tokens.isEmpty()) {
            res.end(tokens.pop());
        }
        return flagMap;
    }

    private static AconfigdFlagInfo readFromProto(ProtoInputStream protoInputStream)
            throws IOException {
        AconfigdFlagInfo.Builder builder = new AconfigdFlagInfo.Builder();
        while (protoInputStream.nextField() != ProtoInputStream.NO_MORE_FIELDS) {
            switch (protoInputStream.getFieldNumber()) {
                case (int) StorageReturnMessage.FlagQueryReturnMessage.PACKAGE_NAME:
                    builder.setPackageName(
                            protoInputStream.readString(
                                    StorageReturnMessage.FlagQueryReturnMessage.PACKAGE_NAME));
                    break;
                case (int) StorageReturnMessage.FlagQueryReturnMessage.FLAG_NAME:
                    builder.setFlagName(
                            protoInputStream.readString(
                                    StorageReturnMessage.FlagQueryReturnMessage.FLAG_NAME));
                    break;
                case (int) StorageReturnMessage.FlagQueryReturnMessage.SERVER_FLAG_VALUE:
                    builder.setServerFlagValue(
                            protoInputStream.readString(
                                    StorageReturnMessage.FlagQueryReturnMessage.SERVER_FLAG_VALUE));
                    break;
                case (int) StorageReturnMessage.FlagQueryReturnMessage.LOCAL_FLAG_VALUE:
                    builder.setLocalFlagValue(
                            protoInputStream.readString(
                                    StorageReturnMessage.FlagQueryReturnMessage.LOCAL_FLAG_VALUE));
                    break;
                case (int) StorageReturnMessage.FlagQueryReturnMessage.BOOT_FLAG_VALUE:
                    builder.setBootFlagValue(
                            protoInputStream.readString(
                                    StorageReturnMessage.FlagQueryReturnMessage.BOOT_FLAG_VALUE));
                    break;
                case (int) StorageReturnMessage.FlagQueryReturnMessage.DEFAULT_FLAG_VALUE:
                    builder.setDefaultFlagValue(
                            protoInputStream.readString(
                                    StorageReturnMessage.FlagQueryReturnMessage
                                            .DEFAULT_FLAG_VALUE));
                    break;
                case (int) StorageReturnMessage.FlagQueryReturnMessage.HAS_SERVER_OVERRIDE:
                    builder.setHasServerOverride(
                            protoInputStream.readBoolean(
                                    StorageReturnMessage.FlagQueryReturnMessage
                                            .HAS_SERVER_OVERRIDE));
                    break;
                case (int) StorageReturnMessage.FlagQueryReturnMessage.HAS_LOCAL_OVERRIDE:
                    builder.setHasLocalOverride(
                            protoInputStream.readBoolean(
                                    StorageReturnMessage.FlagQueryReturnMessage
                                            .HAS_LOCAL_OVERRIDE));
                    break;
                case (int) StorageReturnMessage.FlagQueryReturnMessage.IS_READWRITE:
                    builder.setIsReadWrite(
                            protoInputStream.readBoolean(
                                    StorageReturnMessage.FlagQueryReturnMessage.IS_READWRITE));
                    break;
                default:
                    Slog.w(
                            TAG,
                            "Could not read undefined field: " + protoInputStream.getFieldNumber());
            }
        }
        return builder.build();
    }
}
