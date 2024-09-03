# ServiceManager
init进程 –-> ServiceManager 进程

init 进程读取 servicemanager.rc 脚本，调用service_manager.c的main()方法开始ServiceManager进程的创建,启动 servicemanager 进程
```
service servicemanager /system/bin/servicemanager
    class core animation
    user system
    group system readproc
    critical
    onrestart restart healthd
    onrestart restart zygote
    onrestart restart audioserver
    onrestart restart media
    onrestart restart surfaceflinger
    onrestart restart inputflinger
    onrestart restart drm
    onrestart restart cameraserver
    onrestart restart keystore
    onrestart restart gatekeeperd
    onrestart restart thermalservice
    writepid /dev/cpuset/system-background/tasks
    shutdown critical
```

```
service_manager.c
int main(int argc, char **argv)
{
    //binder_state结构体,用来描述binder驱动的状态。通过binder_state可以得到binder驱动的fd用以操作binder驱动。其中包含： int fd; void *mapped; size_t mapsize;
    struct binder_state *bs;
    ...
    //1:打开binder驱动：调用了自己实现的binder_open方法。申请 128k 字节大小的内存空间。
    bs = binder_open(128*1024);
    if (!bs) {
        ALOGE("failed to open binder driver\n");
        return -1;
    }    
    ...
    //2:注册成为binder服务大管家,成为其守护进程：binder_become_context_manager
    if (binder_become_context_manager(bs)) {
        ALOGE("cannot become context manager (%s)\n", strerror(errno));
        return -1;
    }
    ...
    //3:进入无限循环，处理 client 端发来的请求：调用binder_loop。
    binder_loop(bs, svcmgr_handler);
    return 0;
}
```

native 侧初始化完成后会将 ServiceManagerNative 对象保存在 gDefaultServiceManager 中。

native 通过 `sp<IServiceManager> defaultServiceManager()` 函数获取 native 侧的 service manegr

ServiceManager 主要提供 注册 binder 服务、查询 binder 服务 和 获取 binder 服务。

主要用于跨进程通信，`Map<String, IBinder> sCache` 中存储的是对应服务的 IBinder 对象

SystemService 中启动的相关服务会通过 ServiceManager 将对应的 IBinder 保存在 native 侧，同时在 java 侧的 ServiceManager 中缓存一份