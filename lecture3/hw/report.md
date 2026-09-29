# 项目理解报告

请尽量使用自己的语言回答以下问题。可以引用少量关键代码或伪代码，但不要只粘贴实现。
完成一节后删除该节末尾的待填写标记；本地检查会拒绝仍有未完成章节的报告。

## 1. 图像生命周期与所有权

解释本项目中图像源为什么会复用缓冲区，以及 `cv::Mat` 的普通复制对底层像素数据
意味着什么。说明你的修改让一个 `Frame` 在进入队列后拥有什么，并解释为何后续读取
不会再改变它。

相机一直在高速拍照，而拍出的照片需要内存去储存。但如果持续申请内存会导致速度变慢，所以设置缓冲区并复用。若使用cv:Mat的普通复制，则有两个header，但只有一份像素。此时当新的图像覆盖原来buffer_中的内容时，已经放入列队的Frame的内容也被改变，如当worker取出第0帧处理时，发现数据变为了第1帧内容。而clone（）会进行深拷贝，在堆上申请全新的内存并复制像素数据。因此修改过的Frame在进入列队后有独立的内存，其生命周期与buffer_无关。当frame一轮内容处理完后，会自动销毁释放内存，此时新的clone出的frame产生。

## 2. 并发处理与恰好一次

结合 `BlockingQueue` 的 `push`、`pop` 和 `close` 行为，解释多个 worker 如何分工。
为什么你的实现既不会漏掉已经入队的帧，也不会重复处理同一帧？输入耗尽时，正在等待
以及仍在处理数据的 worker 分别会怎样？

BlockingQueue可理解为一个传送带，其中push指producer将Frame放在传送带上，；而pop指worker从传送带上拿走Frame去处理。为避免多个worker同时拿同一东西，或在producer放置时拿东西。使用std：：lock_guard上锁（即拿东西时必须先碰钥匙，且结束自动放回钥匙）当代码执行到push末尾，lock作为局部变量析构，自动调用unlock，pop中也同样使用。因此一个Frame只能被一个worker处理。当worker处理完frame后，会将其从传送带上拿下来，这样避免重复处理。
push最后有ready_.notify_one()，pop最后有ready_.wait()。如果worker拿到钥匙但发现列队为空，则会调用wait（即解锁并阻塞）此时因已解锁，故producer可将新frame放入，当新frame放入时，push会调用notify_one使worker重新工作。当输入耗尽时，producer调用close()。此时在wait中的worker检查条件closed_ || !queue_.empty()，后执行pop的逻辑，到if (queue_.empty()) return false;此时因队列为空，返回false，跳出循环。而正在处理的worker，处理完后再次调用queue_.pop()，此时队列为空，同上，最终跳出循环。由于pipeline中的producerloop里queue_.close()是将20帧放完后（前while循环）才调用，将closed_ 设为 true，告诉worker无新的frame，而worker会一直处理直到队列为空，故无漏帧。

## 3. 共享统计数据

指出哪些线程会读写 `Statistics`。解释原实现中的竞争为什么可能导致错误结果，并说明
你的同步方案提供了什么保证。还应说明取得快照时为什么是安全的。

producerloop线程调用statistics_.onProduced()计数读取了多少帧。workerloop线程调用了statistics_.onProcessed()计数从队列拿了多少帧，statistics_.onSaved()计数多少图片成功写入output目录，statistics_.onCorrupted()计数多少帧有问题，即图像数据被改。最后main中再次读取这四个计数，检验produced>0保证有输入，produced=processed保证没有漏帧，processed=saved保证处理过的帧都写入硬盘，corrupted=0保证数据没有被破坏。
原实现中先读旧值，后故意睡100微秒，再写回。这个过程中若A读完old=10开始睡觉，在睡觉时B也去读old=10，A醒了，执行value=10+1=11，写回内存，而B也将11写回内存，此时少了一次更新。
为解决问题，在Statistics中加锁mutable std::mutex，并在每个读写操作中都用std::lock_guard，此时多个worker线程，同一时刻只有拿到锁的这个线程才能执行读写操作。同时在snapshot()中也加锁，防止main中刚读完produced，还没有读processed时，worker在处理完成，改变了processed的数值，而此时主线程再读processed会导致这四个计数不是同一时刻，引起报错，因此取得快照时也是安全的。由于 snapshot() 是 const 函数，为了在 const 函数中允许加锁，mutex_ 被声明为 mutable。

## 4. 线程关闭协议

分别描述以下两条路径中的事件顺序，并解释为什么不会发生 `std::terminate`、悬空访问
或永久等待：

1. 调用者执行 `start()` 后显式调用 `wait()`；
2. 调用者执行 `start()` 后不调用 `wait()`，直接让 `Pipeline` 析构。

如果你的实现允许某个生命周期方法被重复调用，也请说明其行为；如果不允许，请说明前置条件。

路径1：调用 start() 后显式调用 wait()。首先执行 producer_.join()，等待producer读完20张图片后，执行queue_.close() 关闭传送带。此后wait中的worker被唤醒，发现传送带关闭，跳出循环，而正在工作的worker处理完后，发现传送带空了，故跳出循环。最后wait()依次执行worker.join()，销毁pipline对象。因此没有提前销毁对象，worker不会访问到已经被销毁的内存，且producer读完图片就关闭传送带，worker不会永久等待。
路径2：由于在析构函数 Pipeline() 里写了一行 wait()，当对象要被销毁时，进入析构函数，遇到wait()，开始执行producer_.join() 和 worker.join()。此时会等待线程执行完毕后，std::thread 对象的状态就会从 joinable 变成 not joinable。析构函数结束，编译器去销毁 producer_ 和 workers_，发现它们不再是 joinable 状态，安全销毁，程序结束。
start()不能重复调用，而wait()可以重复调用。如果第一次start()创建worker之后，状态为joinable，第二次又创建新的worker，赋给workers_，此时原来旧的worker被覆盖，但是原线程没有被回收，会导致程序崩溃。所以加了 started_ 标志，第二次调用直接 return，保证线程只被创建一次。而wait() 内部对每个线程都检查了 joinable()，join() 成功后线程状态会变为 not joinable，不会报错。由于main中调用了wait()，且析构函数pipline中也加了wait() ,若不支持重复调用，程序在离开作用域时触发pipline，再次调用wait()时，会使程序崩溃。


