### 延展

[Android源码分析](https://blog.csdn.net/tianzhaoai/category_9493658.html)

[Android 输入系统](https://blog.csdn.net/tianzhaoai/article/details/130532742?spm=1001.2014.3001.5501)

[Android 图形系统详解](https://blog.csdn.net/tianzhaoai/article/details/128943124?spm=1001.2014.3001.5501)

[AMS、Activity 启动流程详解](https://blog.csdn.net/tianzhaoai/article/details/102861315?spm=1001.2014.3001.5501)

[PMS启动 APK 安装流程详解](https://blog.csdn.net/tianzhaoai/article/details/102842692?spm=1001.2014.3001.5501)

[Launch 桌面启动详解](https://blog.csdn.net/tianzhaoai/article/details/102874987)


### 使用

1. 将项目中的 Android10 代码下载下来
2. 打开AS，通过 Open an Existing Project 打开 android10 下面的 android.ipr文件
3. 将源码只关联本地，将dependecies下面的只留下下面两个。

   <img src=https://github.com/xyTianZhao/AndroidFrameWorks/blob/master/image/1.jpg width=40% />
   <img src=https://github.com/xyTianZhao/AndroidFrameWorks/blob/master/image/2.jpg width=40% />
   <img src=https://github.com/xyTianZhao/AndroidFrameWorks/blob/master/image/3.jpg width=40% />
   <img src=https://github.com/xyTianZhao/AndroidFrameWorks/blob/master/image/4.jpg width=40% />

成功导入之后，就可以愉快的看源码了，速度还是挺快的，如果感觉还是有点卡顿的话，可以将AS安装目录下的的studio.vmoptions调大一些。

```
-Xms2048m
-Xmx4049m
-XX:ReservedCodeCacheSize=500m
-XX:+UseG1GC
-XX:SoftRefLRUPolicyMSPerMB=50
-XX:CICompilerCount=3
```

## 更多

如果需要阅读其他模块的源码的话，可以将整个Android10的源码下载下来，然后将对应的模块拷贝到项目android10目录下，并在android.iml中对应的模块移除配置删掉。比如加入了packages模块，然后将下面这行删除。

```
<excludeFolder url="file://$MODULE_DIR$/packages" />
```

