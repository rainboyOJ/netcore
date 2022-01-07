
connection类

成员函数
request
response
multipart_read 

http的协议

简单的header
chunked 相关知识参考
header要设置为`Transfer-Encoding: chunked`
[HTTP 协议中的 Transfer-Encoding | JerryQu 的小站](https://imququ.com/post/transfer-encoding-header-in-http.html)
multipart



get_content_type 

处理 multipart, urlencode,chunked 请求


工作
- read ?  
- write
- 普通的http请求,无body
- 文件上传
- multi_forlaod 
- 文件下载

FAQ

1. 读取问题,读取会保留上一次的读取吗?
