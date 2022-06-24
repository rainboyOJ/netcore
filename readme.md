## 功能

- 使用epoll(io复用,可以同时处理多个请求)与线程池实现Reactor模型
- 基于小根堆实现的定时器，关闭超时的非活动连接；

## 参考

- [qinguoyi/TinyWebServer: Linux下C++轻量级Web服务器](https://github.com/qinguoyi/TinyWebServer)
- [markparticle/WebServer: C++ Linux WebServer服务器](https://github.com/markparticle/WebServer)
- [zhangwenxiao/Cpp11WebServer: A High Performance HTTP Web Server in C++11](https://github.com/zhangwenxiao/Cpp11WebServer)
## 使用自己的配置文件

拷贝`netcore/__default_config.hpp`到自己项目目录,修改对应的值

在自己的加入`CMakeLists.txt`加入如下的一行

```cmake
set(USER_CONFIG_PATH "${PROJECT_SOURCE_DIR}/__user_config.hpp")
```
## 注意

- 需要创建`config`里的上传与静态文件夹


# 使用cpp编写的oj服务器

- [Nomango/configor: Light weight configuration library for C++](https://github.com/Nomango/configor)
- [redis/hiredis: Minimalistic C client for Redis >= 1.2](https://github.com/redis/hiredis)
- [小白视角：一文读懂社长的TinyWebServer | HU](https://huixxi.github.io/2020/06/02/%E5%B0%8F%E7%99%BD%E8%A7%86%E8%A7%92%EF%BC%9A%E4%B8%80%E6%96%87%E8%AF%BB%E6%87%82%E7%A4%BE%E9%95%BF%E7%9A%84TinyWebServer/#more)


## `core.h` 框架
```
                                                                               +---------------------+
                                                                               |                     |                    +-----------------------+
                                                                               |  Acceptor           |                    |                       |
                                                                               |                     |                    |                       |
 +------------------------+                                                    +---------+-----------+          +--------->     Connection        |
 |                        |                                                              |                      |         |                       |
 |    CallBack            |                                                              |async_accept          |    +----+                       |
 |                        |                                                              |                      |    |    +---+-------------------+
 +------------------------+                                                              v                      |    |        |                      Register to task_queue
 |                        |                                                    +--------------------+           |    |        |                      check to timeOut
 |    CallBackImplBase    |                                                    |                    |           |    |    +---v--------------------+
 |                        |                                                    |  AcceptorAwaiter   |           |    |    |     Async_read         +-----------------+
 +------------------------+                                                    |                    |           |    |    |                        |                 |
                                                 +----------------+            +-+------------------+           |    |    +------------------------+                 |
----------------------------                     | 1.add_epoll    |<-----------+-+   await_suspend  |           |    |                                               |
                                                 | 2.set callBack |              +------------------+           |    |    +------------------------+                 |
 +------------------------+                      |                |              |   resume         +-----------+    |    |     Async_send         +-----------------+
 |                        |                      +----------------+              +------------------+                +--->|                        |                 |
 |    AcceptorCallback    |                                                                                               +------------------------+                 |
 |                        |                                                                                                                                          |
 +------------------------+                                                                                                                                          |
                                                                                                                                                                     |
                                                                                                                                                                     |
                                                                                                                                                                     |
                                                                                                                                                                     |
                                                                                                                                                                     |
                                                                                                                                                                     |
                                                                                                                                                                     |
                                                                                                                                                                     |
               +----------------------+               +---------------------------------------------------------------------+                                        |
               |                      |               |                                                                     |                                        |
               |                      |    task_queue |                                                                     |<---------------------------------------+
               |                      |               |                                                                     |
               |                      |               +---------------------------------------------------------------------+
               |                      |
               |       IoCentext      |
               |                      |
               |                      |
               |                      |                +--------------------------------------------------------------------+
               |                      |                |                                                                    |
               |                      |            run |                                                                    |
               |                      |                |                                                                    |
               +----------------------+                +--------------------------------------------------------------------+
```

```
timer.hpp not use
```

## 功能

- [ ] 连接超时检测
- [ ] 表单提交
- [ ] 文件上传
    - [ ] 尺寸限制
    - [ ] multipart
- [ ] 文件下载
- [ ] websocket(被动响应式)
- [ ] 简易Cache
