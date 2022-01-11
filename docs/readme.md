## 说明

这里写了这个服务器的文档，包括

 - 原理
 - 如何使用

## 总体框架

http_server对于每一个请求都产生一个`connection`,所以你可能想知道connection是如何动作的.

![一次connection的读写转换机制](./drawio/一次connection的读写转换机制.png)

![request](./drawio/request.png)

![connection的process拆分](./drawio/connection的process拆分.png)

