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

package android.aidl.tests;

import static org.hamcrest.core.Is.is;
import static org.hamcrest.core.IsNot.not;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.assertTrue;

import android.aidl.test.trunk.ITrunkStableTest;
import android.aidl.test.trunk.ITrunkStableTest.IMyCallback;
import android.aidl.test.trunk.ITrunkStableTest.MyEnum;
import android.aidl.test.trunk.ITrunkStableTest.MyOtherParcelable;
import android.aidl.test.trunk.ITrunkStableTest.MyParcelable;
import android.aidl.test.trunk.ITrunkStableTest.MyUnion;
import android.os.Binder;
import android.os.IBinder;
import android.os.IServiceCallback;
import android.os.IServiceManager;
import android.os.RemoteException;
import android.os.ServiceManager;
import android.os.ServiceManagerNative;
import com.android.internal.os.BinderInternal;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.ExpectedException;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

class BinderWithLinkToDeathFails extends android.os.Binder {
  @Override
  public void linkToDeath(DeathRecipient recipient, int flags) {
    throw new java.lang.IllegalArgumentException();
  }
}

@RunWith(JUnit4.class)
public class ServiceManagerNativeTests {
  private IServiceManager serviceManager;

  @Before
  public void setUp() throws RemoteException {
    serviceManager =
        ServiceManagerNative.asInterface(Binder.allowBlocking(BinderInternal.getContextObject()));
  }

  @Rule public ExpectedException expectedException = ExpectedException.none();
  @Test(expected = java.lang.IllegalArgumentException.class)
  public void testAddServiceInvalidName() throws RemoteException {
    Binder binder = new Binder();
    serviceManager.addService("InvalidName!!!", binder, false, 0);
  }

  @Test(expected = java.lang.NullPointerException.class)
  public void testAddServiceNullBinder() throws RemoteException {
    serviceManager.addService("validName", null, false, 0);
  }

  @Test(expected = android.os.RemoteException.class)
  public void testUnregisterForNotificationsNotSupported() throws RemoteException {
    serviceManager.unregisterForNotifications("validName", null);
  }

  @Test(expected = android.os.RemoteException.class)
  public void testTryUnregisterServiceNotSupported() throws RemoteException {
    serviceManager.tryUnregisterService("validName", null);
  }

  @Test(expected = android.os.RemoteException.class)
  public void testRegisterClientCallbackNotSupported() throws RemoteException {
    serviceManager.registerClientCallback("validName", null, null);
  }
}
