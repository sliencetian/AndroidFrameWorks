package com.android.server.wm;

/**
 * Class that is used to generate an instance of the WM global lock. We are only doing this because
 * we need a class for the pattern used in frameworks/base/services/core/Android.bp for CPU boost
 * in the WM critical section.
 * <br/>
 * 用于生成 WM 全局锁实例的类。
 * 我们这样做只是因为我们需要一个用于 frameworks/base/services/core/Android.bp 中使用的模式的类，
 * 以在WM 关键部分中提高CPU 性能。
 */
public class WindowManagerGlobalLock {
}
