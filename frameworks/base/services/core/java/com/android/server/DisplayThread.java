/*
 * Copyright (C) 2013 The Android Open Source Project
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

package com.android.server;

import android.os.Handler;
import android.os.Process;
import android.os.Trace;

/**
 * Shared singleton foreground thread for the system.  This is a thread for
 * operations that affect what's on the display, which needs to have a minimum
 * of latency.  This thread should pretty much only be used by the WindowManager,
 * DisplayManager, and InputManager to perform quick operations in real time.<br/><br/>
 * 系统的共享单例前台线程。<br/>
 * 这是一个用于影响显示内容的操作的线程，需要具有最小的延迟。<br/>
 * 这个线程应该只被 WindowManager、DisplayManager 和 InputManager 用来执行实时的快速操作。<br/>
 */
public final class DisplayThread extends ServiceThread {
    private static DisplayThread sInstance;
    private static Handler sHandler;

    private DisplayThread() {
        // DisplayThread runs important stuff, but these are not as important as things running in
        // AnimationThread. Thus, set the priority to one lower.
        super("android.display", Process.THREAD_PRIORITY_DISPLAY + 1, false /*allowIo*/);
    }

    private static void ensureThreadLocked() {
        if (sInstance == null) {
            sInstance = new DisplayThread();
            sInstance.start();
            sInstance.getLooper().setTraceTag(Trace.TRACE_TAG_SYSTEM_SERVER);
            sHandler = new Handler(sInstance.getLooper());
        }
    }

    public static DisplayThread get() {
        synchronized (DisplayThread.class) {
            ensureThreadLocked();
            return sInstance;
        }
    }

    public static Handler getHandler() {
        synchronized (DisplayThread.class) {
            ensureThreadLocked();
            return sHandler;
        }
    }
}
