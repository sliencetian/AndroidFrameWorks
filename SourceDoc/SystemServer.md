# SystemServer
```
Zygote调用startSystemServer创建system_server进程
    SS调用handleSystemServerProcess完成自己的使命
    handleSystemServerProcess抛出异常，最终调用com.android.server.SystemServer的main函数
    main加载libandroid_server.so并调用native_init方法
    在后面的3个关键方法中，启动一些系统服务
        startBootstrapServices();
        startCoreServices();
        startOtherServices();
    进入消息循环，等待请求
```

## LocalServices
这个类的使用方式与ServiceManager类似，只是这里注册的服务不是Binder对象。
而是 SystemServer 进程中的 java 服务，只能在 SystemServer 同一进程中使用。
