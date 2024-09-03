# ActivityTaskManagerService

## TaskRecord


## RecentTasks

AMS systemReady 后，会调用 RecentTasks 的 onSystemReadyLocked 方法加载 Recent 页面
```
void onSystemReadyLocked() {
    // com.android.systemui.recents.RecentsActivity
    loadRecentsComponent(mService.mContext.getResources());
    mTasks.clear();
}
```

## Task 磁盘读写
TaskPersister 在 ActivityStackSupervisor 中创建，传递给 RecentTasks
TaskPersister 初始化时创建 LazyTaskWriterThread 线程，死循环处理相关事件
### 读
当 UserUnLock 后会将磁盘中缓存的 taskIds 和 RecentTask 读取到内存中
```
UserController.loadUserRecents()->ams.atms.loadRecentTasksForUser(userId)
->mRecentTasks.loadUserRecentsLocked()
    // 读取 task ids
    ->loadPersistedTaskIdsForUserLocked()->mTaskPersister.loadPersistedTaskIdsForUser(userId)
    // 读取 taskinfo
    ->mTaskPersister.restoreTasksForUserLocked(userId, preaddedTasks)
```
### 写
写 taskids 到 data/system_de/{userid}/persisted_taskIds.txt 文件中
```
UserController.loadUserRecents()->SystemServiceManager().unlockUser(userId)
->ATMS.onUnlockUser()->mStackSupervisor.onUserUnlocked()->mPersisterQueue.startPersisting()
->mLazyTaskWriterThread.start()

private class LazyTaskWriterThread extends Thread {
    public void run() {
        while (true) {
            final boolean probablyDone;
            synchronized (PersisterQueue.this) {
                probablyDone = mWriteQueue.isEmpty();
            }
            for (int i = mListeners.size() - 1; i >= 0; --i) {
                // 调用 TaskPersister#onPreProcessItem 方法，将 task ids 写入文件保存
                mListeners.get(i).onPreProcessItem(probablyDone);
            }
            // 当 mWriteQueue 为空时会调用 wait() 函数挂起，等待事件唤醒
            processNextItem();
        }
    }
    private void processNextItem(){
        item = mWriteQueue.remove(0);
        item.process();
    }
}
```
写 taskInfo
当 task 发生变化时，会调用 RecentTasks 的 notifyTaskPersisterLocked 方法写入 data/system_ce/{userid}/recent_tasks 文件夹下面
```
// 向 mWriteQueue 添加 TaskWriteQueueItem 任务，并且通过 notify 唤醒挂起的线程
RecentTasks.notifyTaskPersisterLocked(task, flush)->mTaskPersister.wakeup(task,flush)
->mPersisterQueue.addItem(new TaskWriteQueueItem(task, mService), flush)
->mWriteQueue.add(item)->notify()

private static class TaskWriteQueueItem implements PersisterQueue.WriteQueueItem {
    public void process() {
        StringWriter stringWriter = saveToXml(task);
        AtomicFile atomicFile = new AtomicFile(new File(userTasksDir,String.valueOf(task.taskId) + TASK_FILENAME_SUFFIX));
        file = atomicFile.startWrite();
        file.write(stringWriter.toString().getBytes());
        file.write('\n');
        atomicFile.finishWrite(file);
    }
}
```



