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

import android.net.LocalSocket;
import android.net.LocalSocketAddress;
import android.util.Slog;

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.io.InputStream;

/** @hide */
public class AconfigdClientSocketImpl implements AconfigdClientSocket {

    private static String TAG = "AconfigdClientSocketImpl";

    private LocalSocket mLocalSocket;
    private LocalSocketAddress mLocalSocketAddress;

    public AconfigdClientSocketImpl(LocalSocketAddress address) {
        mLocalSocket = new LocalSocket();
        mLocalSocketAddress = address;
    }

    AconfigdClientSocketImpl() {
        this(new LocalSocketAddress("aconfigd", LocalSocketAddress.Namespace.RESERVED));
    }

    /**
     * Connect the socket
     *
     * @hide
     */
    public void connect() throws IOException {
        if (mLocalSocket.isConnected()) return;
        mLocalSocket.connect(mLocalSocketAddress);
    }

    /**
     * send request
     *
     * @param requests stream of requests
     * @hide
     */
    @Override
    public InputStream send(byte[] requests) {
        if (!mLocalSocket.isConnected()) {
            try {
                connect();
            } catch (IOException e) {
                Slog.e(TAG, "fail to connect to the server socket", e);
                return null;
            }
        }

        DataInputStream inputStream = null;
        DataOutputStream outputStream = null;
        try {
            inputStream = new DataInputStream(mLocalSocket.getInputStream());
            outputStream = new DataOutputStream(mLocalSocket.getOutputStream());
        } catch (IOException ioe) {
            Slog.e(TAG, "failed to get local socket iostreams", ioe);
            return null;
        }

        // send requests
        try {
            outputStream.writeInt(requests.length);
            outputStream.write(requests);
            Slog.i(TAG, "flag requests sent to aconfigd");
        } catch (IOException ioe) {
            Slog.e(TAG, "failed to send requests to aconfigd", ioe);
            return null;
        }

        // read return
        try {
            int num_bytes = inputStream.readInt();
            Slog.i(TAG, "received " + num_bytes + " bytes back from aconfigd");
            return mLocalSocket.getInputStream();
        } catch (IOException ioe) {
            Slog.e(TAG, "failed to read requests return from aconfigd", ioe);
            return null;
        }
    }

    /**
     * Close the local socket
     *
     * @hide
     */
    @Override
    public void close() {
        try {
            mLocalSocket.shutdownInput();
            mLocalSocket.shutdownOutput();
            mLocalSocket.close();
        } catch (IOException e) {
            Slog.e(TAG, "failed to close socket", e);
        }
    }

}
