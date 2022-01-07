[ly-web/PicoHTTPParser: PicoHTTPParser 是一个小的、原始的、快速的HTTP请求/响应解析器。](https://github.com/ly-web/PicoHTTPParser)

主要学习`phr_parse_request`的使用

源代码就不看了

是否可以分段解析?

phr_parse_request

参数
```plaintext
buf 解析的buffer的起始地址
buflen buf解析的长度
&method  返回解析后 方法GET POST ...
&method_len 字符串长度
&path 路径
&path_len 路径长度
&minor_version 解析的 header 版本
headers 存header
&num_headers 最后的header的长度
prevbuflen 上一次解析的长度
)
```

返回值
```plaintext
 > 0 解析完成
 -1 发生错误
 -2 继续解析
```
