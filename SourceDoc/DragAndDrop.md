# DragAndDrop 拖拽
# 初始化
```java
class WindowManagerService {
    final H mH = new H();
    final DragDropController mDragDropController;
    private WindowManagerService(...) {
        ......
        mDragDropController = new DragDropController(this, mH.getLooper());
        ......
    }
    void updatePointerIcon(IWindow client) {
        ......
        synchronized (mGlobalLock) {
            if (mDragDropController.dragDropActiveLocked()) {
                // Drag cursor overrides the app cursor.
                return;
            }
        }
        ......
    }
    private final class LocalService extends WindowManagerInternal {
        @Override
        public void registerDragDropControllerCallback(IDragDropCallback callback) {
            mDragDropController.registerCallback(callback);
        }
    }
}
```
# 启动
```java
class View {
    public final boolean startDragAndDrop(ClipData data, DragShadowBuilder shadowBuilder, Object myLocalState, int flags) {
        ......
        if (data != null) {
            /***  data 数据为跨进程传递做准备 */
            data.prepareToLeaveProcess((flags & View.DRAG_FLAG_GLOBAL) != 0);
        }
        ......
        /*** 获取拖拽窗口的大小和触碰中心点 */
        shadowBuilder.onProvideShadowMetrics(shadowSize, shadowTouchPoint);
        final ViewRootImpl root = mAttachInfo.mViewRootImpl;
        /*** 创建 SurfaceControl 用于将 drag 的图像提交只 SurfaceFlinger */
        final SurfaceSession session = new SurfaceSession();
        final SurfaceControl surfaceControl = new SurfaceControl.Builder(session)
                .setName("drag surface")
                .setParent(root.getSurfaceControl())
                .setBufferSize(shadowSize.x, shadowSize.y)
                .setFormat(PixelFormat.TRANSLUCENT)
                .build();
        /*** 创建 java 层用于绘制的 surface */
        final Surface surface = new Surface();
        surface.copyFrom(surfaceControl);
        ......
        final Canvas canvas = surface.lockCanvas(null);
        try {
            canvas.drawColor(0, PorterDuff.Mode.CLEAR);
            /*** 绘制拖拽内容 */
            shadowBuilder.onDrawShadow(canvas);
        } finally {
            surface.unlockCanvasAndPost(canvas);
        }
        ......
        /*** 开始拖拽 */
        token = mAttachInfo.mSession.performDrag(
                mAttachInfo.mWindow, flags, surfaceControl, root.getLastTouchSource(),
                shadowSize.x, shadowSize.y, shadowTouchPoint.x, shadowTouchPoint.y, data);
        ......
    }
}
class Session {
    private final DragDropController mDragDropController;
    public Session(WindowManagerService service, IWindowSessionCallback callback) {
        mDragDropController = service.mDragDropController;
    }

    @Override
    public IBinder performDrag(IWindow window, int flags, SurfaceControl surface, int touchSource, float touchX, float touchY, float thumbCenterX, float thumbCenterY, ClipData data) {
        return mDragDropController.performDrag(mSurfaceSession, callerPid, callerUid, window,
                    flags, surface, touchSource, touchX, touchY, thumbCenterX, thumbCenterY, data);
    }
    @Override
    public void cancelDragAndDrop(IBinder dragToken, boolean skipAnimation) {
        mDragDropController.cancelDragAndDrop(dragToken, skipAnimation);
    }

    @Override
    public void dragRecipientEntered(IWindow window) {
        mDragDropController.dragRecipientEntered(window);
    }

    @Override
    public void dragRecipientExited(IWindow window) {
        mDragDropController.dragRecipientExited(window);
    }
}
class DragDropController {
    IBinder performDrag(SurfaceSession session, int callerPid, int callerUid, IWindow window, int flags, SurfaceControl surface, int touchSource, float touchX, float touchY, float thumbCenterX, float thumbCenterY, ClipData data) {

        /*** 空实现，默认返回 true */
        final IBinder dragToken = new Binder();
        final boolean callbackResult = mCallback.get().prePerformDrag(window, dragToken,
                touchSource, touchX, touchY, thumbCenterX, thumbCenterY, data);
        try {
            synchronized (mService.mGlobalLock) {
                try {
                    ......
                    /*** 正在拖拽，屏蔽其他起始拖拽 */
                    if (dragDropActiveLocked()) {
                        return null;
                    }
                    // 当前 window 合法，并且可以接收 touch 事件
                    final WindowState callingWin = mService.windowForClientLocked(null, window, false);
                    if (callingWin == null || callingWin.cantReceiveTouchInput()) {
                        return null; 
                    }
                    ......
                    /*** 拖拽背景是否透明 */
                    final float alpha = (flags & View.DRAG_FLAG_OPAQUE) == 0 ? DRAG_SHADOW_ALPHA_TRANSPARENT : 1;
                    final IBinder winBinder = window.asBinder();
                    IBinder token = new Binder();
                    /*** 创建本次拖拽的状态机 */
                    mDragState = new DragState(mService, this, token, surface, flags, winBinder);
                    surface = null;
                    mDragState.mPid = callerPid;
                    mDragState.mUid = callerUid;
                    mDragState.mOriginalAlpha = alpha;
                    mDragState.mToken = dragToken;
                    mDragState.mDisplayContent = displayContent;

                    final Display display = displayContent.getDisplay();
                    /*** 注册输入拦截，切换窗口焦点 */
                    if (!mCallback.get().registerInputChannel(
                            mDragState, display, mService.mInputManager,
                            callingWin.mInputChannel)) {
                        Slog.e(TAG_WM, "Unable to transfer touch focus");
                        return null;
                    }

                    mDragState.mData = data;
                    /**
                     * 调用每个可见的窗口会话，通知其有关拖动的信息
                     * notify DragEvent.ACTION_DRAG_STARTED
                     */
                    mDragState.broadcastDragStartedLocked(touchX, touchY);
                    mDragState.overridePointerIconLocked(touchSource);
                    // remember the thumb offsets for later
                    mDragState.mThumbOffsetX = thumbCenterX;
                    mDragState.mThumbOffsetY = thumbCenterY;

                    // Make the surface visible at the proper location
                    final SurfaceControl surfaceControl = mDragState.mSurfaceControl;
                    if (SHOW_LIGHT_TRANSACTIONS) Slog.i(TAG_WM, ">>> OPEN TRANSACTION performDrag");

                    /*** 显示 drag 窗口 */
                    final SurfaceControl.Transaction transaction = mDragState.mTransaction;
                    transaction.setAlpha(surfaceControl, mDragState.mOriginalAlpha);
                    transaction.setPosition(
                            surfaceControl, touchX - thumbCenterX, touchY - thumbCenterY);
                    transaction.show(surfaceControl);
                    displayContent.reparentToOverlay(transaction, surfaceControl);
                    callingWin.scheduleAnimation();

                    if (SHOW_LIGHT_TRANSACTIONS) {
                        Slog.i(TAG_WM, "<<< CLOSE TRANSACTION performDrag");
                    }

                    /**
                     * 这里会先发送 {@link android.view.DragEvent#ACTION_DRAG_ENTERED}
                     * 在发送 {@link android.view.DragEvent#ACTION_DRAG_LOCATION}
                     */
                    mDragState.notifyLocationLocked(touchX, touchY);
                } finally {
                    if (surface != null) {
                        surface.release();
                    }
                    if (mDragState != null && !mDragState.isInProgress()) {
                        mDragState.closeLocked();
                    }
                }
            }
            return dragToken;    // success!
        } finally {
            mCallback.get().postPerformDrag();
        }
    }
}
```
# 更新拖拽内容
```java
class View {
    /**
     *  更新正在进行的拖放操作的拖动阴影。
     *  通过 surface 提交给 surface flinger 进行绘制
     */
    public final void updateDragShadow(DragShadowBuilder shadowBuilder) {
        ......
        /*** 在 {@link #startDragAndDrop(ClipData, DragShadowBuilder, Object, int)} 中创建的  */
        Canvas canvas = mAttachInfo.mDragSurface.lockCanvas(null);
        try {
            canvas.drawColor(0, PorterDuff.Mode.CLEAR);
            shadowBuilder.onDrawShadow(canvas);
        } finally {
            mAttachInfo.mDragSurface.unlockCanvasAndPost(canvas);
        }
        ......
    }
}
```