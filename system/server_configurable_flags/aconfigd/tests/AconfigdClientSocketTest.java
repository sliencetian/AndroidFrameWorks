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

import static org.junit.Assert.assertEquals;

import android.aconfigd.AconfigdClientSocket;
import android.aconfigd.AconfigdClientSocketImpl;
import android.net.LocalServerSocket;
import android.net.LocalSocket;
import android.net.LocalSocketAddress;
import android.util.proto.ProtoInputStream;
import android.util.proto.ProtoOutputStream;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.InputStream;

@RunWith(JUnit4.class)
public class AconfigdClientSocketTest {

    private String mTestAddress = "android.aconfigd.test";
    private LocalServerSocket mLocalServer;
    AconfigdClientSocketImpl mClientLocalSocket;
    private LocalSocket mServerLocalSocket;

    @Before
    public void setUp() throws Exception {
        mLocalServer = new LocalServerSocket(mTestAddress);
        mClientLocalSocket = new AconfigdClientSocketImpl(new LocalSocketAddress(mTestAddress));
        mClientLocalSocket.connect();
        mServerLocalSocket = mLocalServer.accept();
    }

    @After
    public void tearDown() throws Exception {
        mServerLocalSocket.close();
        mLocalServer.close();
        mClientLocalSocket.close();
    }

    @Test
    public void testSend() throws Exception {
        long fieldFlags =
                ProtoOutputStream.FIELD_COUNT_SINGLE | ProtoOutputStream.FIELD_TYPE_STRING;
        long fieldId = ProtoOutputStream.makeFieldId(1, fieldFlags);

        // client request message
        String testReqMessage = "request test";
        ProtoOutputStream request = new ProtoOutputStream();
        request.write(fieldId, testReqMessage);

        // server return message
        String testRevMessage = "received test";
        ProtoOutputStream serverReturn = new ProtoOutputStream();
        serverReturn.write(fieldId, testRevMessage);
        DataOutputStream outputStream = new DataOutputStream(mServerLocalSocket.getOutputStream());
        outputStream.writeInt(serverReturn.getRawSize());
        outputStream.write(serverReturn.getBytes());

        // validate client received
        InputStream inputStream = mClientLocalSocket.send(request.getBytes());
        ProtoInputStream clientRev = new ProtoInputStream(inputStream);
        clientRev.nextField();
        assertEquals(testRevMessage, clientRev.readString(fieldId));

        // validate server received
        DataInputStream dataInputStream = new DataInputStream(mServerLocalSocket.getInputStream());
        dataInputStream.readInt();
        ProtoInputStream serverRev = new ProtoInputStream(dataInputStream);
        serverRev.nextField();
        assertEquals(testReqMessage, serverRev.readString(fieldId));
    }
}
