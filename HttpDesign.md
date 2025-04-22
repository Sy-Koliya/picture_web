# HttpServer
 继承BaseSocket    
功能 用于启动服务端，接收网页端来的Http请求  
# HttpRequest
继承BaseSocket
通过状态机实现
利用chain buffer 的separate 分割http请求体
支持http1.1和1.0  长连接和短连接
每一次触发读事件的时候，将数据写入缓冲区，进行状态机的判断
case 1 Http_Header_Read
正在获取头部内容，调用缓冲区separate，检查是否包含/r/n/r/n 如果有则进入状态2,同时清空对应的长度
case 2 Http_Parser_Header  (中间态)
   subcase 1 解析失败 头格式错误，报错，关闭对端，报错可能 (1)格式错误 (2)不包含对应的url 
   subcase 2 解析成功，包含 Content-length或者 Chunked 进入对应解析 
             状态设置为Http_len_parser(中间态或者现行态) 或者 Http_Chunked_parser(中间态或者现行态) 如果解析完毕 跳到 case 3
   subcase 3 不包含对应的Content-length或者 Chunked 跳到 case 3
case 3 HttpCallback (中间态)
      调用api分发函数
      如果http协议为1.1 keep-alive，将状态重新设置为 Http_Header_Read(case 1)


补充：存在请求体内容为空的情况，因为有一些api是get请求，所有api要进一步对body进行判断
      Content-length或者Chunked用union 
      加入超时机制，每当调用read时，更新当前的last_recive，创建一个Http_Alive_Manager类管理，利用优先队列，每一秒检测一次