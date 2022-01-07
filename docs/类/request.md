一个用来处理请求的类

功能区

文件上传

### 1 头解析
get_header_value(key)

state_
conn_type

### 2 multipart 相关

multipart_field 什么时候被设定
set_multipart_headers
get_multipart_field_name() ?
save_multipart_key_value()
update_multipart_value
get_multipart_value_by_key 得到value
handle_multipart_key_value 转成form_url_map
is_multipart_file
set_multipart_headers 拷贝

### 3 range_file设定

### 4 解析 query

parse_query
get_query_value(size) //得到第n个值????
get_query_value(string_view) // int float long
std::string_view


### 5 解析 form_url

### 6 文件处理

open_upload_file  创建文件


### 7 缓存模型

update_and_expand_size 核心 : cur_size_ 增加
set_body_len
set_left_body_size
fit_size
expand_size
reduce_left_body_size
left_body_len()
cur_size_ 当前剩余的buf size
在写入buf时如何怎么增长?
在读取一定的buffer 后,减少left_body_len_

```plaintext
函数
 +request(response & res)
 +set_conn(conn_type conn)
 +get_raw_conn()
 +get_conn()
 +is_conn_alive()
 +get_weak_base_conn()
 +parse_header(std::size_t last_len,size_t start=0)
 +check_request()
 +raw_url()
 +set_body_len(size_t len)
 +total_len()
 +header_len() const
 +body_len() const
 +has_recieved_all()
 +has_recieved_all_part()
 +at_capacity()
 +at_capacity(size_t size)
 +current_size() const
 +left_size()
 +update_size(size_t size)
 +update_and_expand_size(size_t size)
 +buffer()
 +data()
 +last_len() const
 +req_buf()
 +head()
 +body()
 +set_left_body_size(size_t size)
 +body() const
 +current_part() const
 +buffer(size_t size) const
 +reset()
 +fit_size()
 +expand_size()
 +has_body() const
 +is_http11()
 +minor_version()
 +left_body_len() const
 +body_finished()
 +is_chunked() const
 +has_gzip() const
 +reduce_left_body_size(size_t size)
 +left_body_size()
 +set_current_size(size_t size)
 +get_header_value(std::string_view key) const
 +get_headers()
 +get_multipart_field_name(const std::string & field_name) const
 +save_multipart_key_value(const std::string & key,const std::string & value)
 +update_multipart_value(std::string key,const char * buf,size_t size)
 +get_multipart_value_by_key1(const std::string & key)
 +handle_multipart_key_value()
 +is_multipart_file() const
 +set_multipart_headers(const multipart_headers & headers)
 +parse_query(std::string_view str)
 +parse_form_urlencoded()
 +parse_chunked(size_t bytes_transferred)
 +get_method() const
 +get_url() const
 +get_res_path() const
 +get_relative_filename() const
 +get_filename_from_path() const
 +get_mime(std::string_view filename) const
 +get_form_url_map() const
 +set_state(data_proc_state state)
 +get_state() const
 +set_part_data(std::string_view data)
 +get_part_data() const
 +set_http_type(content_type type)
 +get_content_type() const
 +queries() const
 +get_query_value(size_t n)
 +get_query_value(std::string_view key)
 +get_query_value(std::string_view key)
 +uncompress(std::string_view str)
 +uncompress()
 +open_upload_file(const std::string & filename)
 +write_upload_data(const char * data,size_t size)
 +close_upload_file()
 +get_upload_files() const
 +get_file()
 +get_cookies() const
 +set_range_flag(bool flag)
 +is_range() const
 +set_range_start_pos(std::string_view range_header)
 +get_range_start_pos() const
 +save_request_static_file_size(std::int64_t size)
 +get_request_static_file_size() const
 +on(data_proc_state event_type,event_call_back && event_call_back)
 +call_event(data_proc_state event_type)
 +set_aspect_data(T &&...data)
 +set_aspect_data(std::vector<std::string> && data)
 +get_aspect_data()
 +set_last_len(size_t len)
 +set_validate(size_t max_header_len,check_header_cb check_headers)
 +resize_double()
 +resize(size_t size)
 +copy_method_url_headers()
 +check_gzip()
      [members]
 +MaxSize
 +conn_
 +res_
 +buf_
 +num_headers_
 +headers_
 +method_
 +method_len_
 +url_
 +url_len_
 +minor_version_
 +header_len_
 +body_len_
 +raw_url_
 +method_str_
 +url_str_
 +cookie_str_
 +copy_headers_
 +cur_size_
 +left_body_len_
 +last_len_
 +queries_
 +form_url_map_
 +multipart_form_map_
 +has_gzip_
 +gzip_str_
 +is_chunked_
 +max_header_len_
 +check_headers_
 +state_
 +part_data_
 +http_type_
 +multipart_headers_
 +last_multpart_key_
 +files_
 +utf8_character_params_
 +utf8_character_pathinfo_params_
 +range_start_pos_
 +is_range_resource_
 +static_resource_file_size_
 +aspect_data_
 +event_call_backs_
```
