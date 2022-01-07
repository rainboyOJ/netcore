http 协议中cookie的介绍


一图流

```


    request              reponse    
+------------+       +------------+ 
|            |       |            | 
|  Cookie:xx |       |Set-Cookie:x| 
|            |       |            | 
+------------+       +------------+ 



```


cookie的属性
一般cookie所具有的属性，包括：

Domain：域，表示当前cookie所属于哪个域或子域下面。

对于服务器返回的Set-Cookie中，如果没有指定Domain的值，那么其Domain的值是默认为当前所提交的http的请求所对应的主域名的。比如访问 http://www.example.com，返回一个cookie，没有指名domain值，那么其为值为默认的www.example.com。

Path：表示cookie的所属路径。

Expire time/Max-age：表示了cookie的有效期。expire的值，是一个时间，过了这个时间，该cookie就失效了。或者是用max-age指定当前cookie是在多长时间之后而失效。如果服务器返回的一个cookie，没有指定其expire time，那么表明此cookie有效期只是当前的session，即是session cookie，当前session会话结束后，就过期了。对应的，当关闭（浏览器中）该页面的时候，此cookie就应该被浏览器所删除了。

secure：表示该cookie只能用https传输。一般用于包含认证信息的cookie，要求传输此cookie的时候，必须用https传输。

httponly：表示此cookie必须用于http或https传输。这意味着，浏览器脚本，比如javascript中，是不允许访问操作此cookie的。

[Http协议中Cookie详细介绍 - 李小菜丶 - 博客园](https://www.cnblogs.com/bq-med/p/8603664.html)
