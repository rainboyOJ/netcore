#pragma once
#include <fstream>
#include "picohttpparser.h"
#include "utils.hpp"
#include "multipart_reader.hpp"
#ifdef CINATRA_ENABLE_GZIP
#include "gzip.hpp"
#endif
#include "define.h"
#include "upload_file.hpp"
#include "url_encode_decode.hpp"
#include "mime_types.hpp"
#include "response.hpp"


//#include "session.hpp"
//#include "session_manager.hpp"

namespace netcore {

    //表示数据处理的状态
    enum class data_proc_state : int8_t { 
        data_begin,     //数据开始
        data_continue,  //继续
        data_end,       //结束
        data_all_end,   //全部结束
        data_close,     //关闭
        data_error      //错误
    };

    class base_connection;
    class connection;
    class request;

    using conn_type       = std::weak_ptr<connection>; //使用weak_ptr 不会产生循环引用
    using check_header_cb = std::function<bool(request&)>;

    class request {
    public:
        using event_call_back = std::function<void(request&)>;
        
        request(response& res) : res_(res){
            buf_.resize(1024);  //基础的buffer大小
        }

        void set_conn(conn_type conn) { //指向connection的weak_ptr
            conn_ = std::move(conn);
        }

        //得到 raw connection ptr
        connection* get_raw_conn() {
            if (conn_.expired())
                return nullptr;

            if (auto base_conn = conn_.lock(); base_conn != nullptr) {
                return (connection*)(base_conn.get());
            }
            else {
                return nullptr;
            }
        }

        //得到一个 shared_ptr 的 connection
        std::shared_ptr<connection> get_conn() {
            //static_assert(std::is_same_v<T, cinatra::SSL> || std::is_same_v<T, cinatra::NonSSL>, "invalid socket type, must be SSL or NonSSL");
            if (conn_.expired())
                return nullptr;

            if (auto base_conn = conn_.lock(); base_conn != nullptr) {
                return std::static_pointer_cast<connection>(base_conn);
                //return base_conn;
            }
            else {
                return nullptr;
            }
        }

        //connection 是否还存活
        bool is_conn_alive() {
            auto base_conn = conn_.lock();
            return base_conn != nullptr;
        }

        // 得到 weak_ptr 指针
        conn_type get_weak_base_conn() {
            return conn_;
        }

        // Author:Rainboy
        //int parse_header_expand_last_len(std::size_t exp_last_len){
            //LOG_DEBUG("last_len_ %d",last_len_);
            //int ret  = parse_header(last_len_);
            //last_len_ += exp_last_len;
            //return ret;
        //}

        // last_len_ 应该从0开始
        // 解析 headers
        int parse_header(std::size_t last_len, size_t start=0) {
            using namespace std::string_view_literals;
            if(!copy_headers_.empty()) //清空copy_headers_
                copy_headers_.clear();

            num_headers_ = sizeof(headers_) / sizeof(headers_[0]); // 
            header_len_ = phr_parse_request(buf_.data(), cur_size_, &method_,
                &method_len_, &url_, &url_len_,
                &minor_version_, headers_, &num_headers_, last_len);

            if (cur_size_ > max_header_len_) { //header buff_的最大空间,那它不应该在解析前判断吗? TODO
                return -1;
            }

            if (header_len_ <0 ) //解析的返回值
                return header_len_;

            if (!check_request()) { // 调用check_request这个函数
                return -1;
            }

            check_gzip(); //修改 has_gzip_ 的值
            auto header_value = get_header_value("content-length");
            if (header_value.empty()) { //是否是chunked
                auto transfer_encoding = get_header_value("transfer-encoding");
                if (transfer_encoding == "chunked"sv) {
                    is_chunked_ = true;
                }
                
                body_len_ = 0; // body的长度设为0
            }
            else {
                set_body_len(atoll(header_value.data()));
            }

            auto cookie = get_header_value("cookie"); 
            if (!cookie.empty()) {
                cookie_str_ = std::string(cookie.data(), cookie.length());
            }

            //parse url and queries
            raw_url_ = {url_, url_len_};
            size_t npos = raw_url_.find('/');
            if (npos == std::string_view::npos) //解析不正确,没有url
                return -1;

            size_t pos = raw_url_.find('?');
            if(pos!=std::string_view::npos){ //解析query
                queries_ = parse_query(std::string_view{raw_url_}.substr(pos+1, url_len_-pos-1));
                url_len_ = pos;
            }

            return header_len_;
        }

        //调用check_request这个函数
        bool check_request() {
            if (check_headers_) {
                return check_headers_(*this);
            }

            return true;
        }

        std::string_view raw_url() {
            return raw_url_;
        }

        void set_body_len(size_t len) {
            body_len_ = len;
            left_body_len_ = body_len_; // 什么是left_body_len_
        }

        size_t total_len() {
            return header_len_ + body_len_;
        }

        size_t header_len() const{
            return header_len_;
        }

        size_t body_len() const {
            return  body_len_;
        }

        bool has_recieved_all() { //全部接收完
            return (total_len() <= current_size());
        }

        /**
         * @brief 读取到所有的 multipart 数据
         */
        bool has_recieved_all_part() {
            return (body_len_ == cur_size_ - header_len_);
        }

        bool at_capacity() { //超过容量
            return (header_len_ + body_len_) > MaxSize;
        }

        bool at_capacity(size_t size) {
            return size > MaxSize;
        }

        size_t current_size() const{
            return cur_size_;
        }

        size_t left_size() { //剩余空间
            return buf_.size() - cur_size_;
        }

        //增加 cur_size_ 标明的大小
        //true 表明超过了最大的基本容量
        bool update_size(size_t size) { 
            cur_size_ += size;
            if (cur_size_ > MaxSize) {
                return true;
            }

            return false;
        }

        // 返回 true 表示达到最大容量
        bool update_and_expand_size(size_t size) {
            if (update_size(size)) { //at capacity
                return true;  
            }

            if (cur_size_ >= buf_.size())
                resize_double(); //增加容量

            return false;
        }

        //返回buffer 可以写的位置
        char* buffer() {
            return &buf_[cur_size_];
        }

        auto get_cur_size_ () const {
            return cur_size_;
        }

        //原始数据
        const char* data() {
            return buf_.data();
        }

        const size_t last_len() const {
            return last_len_;
        }

        //把 请求的buf 转化成string_view
        std::string_view req_buf() {
            return std::string_view(buf_.data() + last_len_, total_len());
        }

        // header 的buf
        std::string_view head() {
            return std::string_view(buf_.data() + last_len_, header_len_);
        }

        // body 的buf
        std::string_view body() {
            return std::string_view(buf_.data() + last_len_ + header_len_, body_len_);
        }

        //TODO ?
        void set_left_body_size(size_t size) {
            left_body_len_ = size;
        }

        std::string_view body() const{
#ifdef CINATRA_ENABLE_GZIP
            if (has_gzip_&&!gzip_str_.empty()) {
                return { gzip_str_.data(), gzip_str_.length() };
            }
#endif
            return std::string_view(&buf_[header_len_], body_len_);
        }

        // TODO ?
        const char* current_part() const {
            return &buf_[header_len_];
        }

        const char* buffer(size_t size) const {
            return &buf_[size];
        }

        void reset() { //重置
            cur_size_ = 0;
            for (auto& file : files_) {
                file.close();
            }
            files_.clear();
            is_chunked_                = false;
            state_                     = data_proc_state::data_begin;
            part_data_                 = {};
            utf8_character_params_.clear();
            utf8_character_pathinfo_params_.clear();
            queries_.clear();
            cookie_str_.clear();
            form_url_map_.clear();
            multipart_form_map_.clear();
            is_range_resource_         = false;
            range_start_pos_           = 0;
            static_resource_file_size_ = 0;
            last_len_ = 0;
            copy_headers_.clear();
        }

        void fit_size() { // 增长,有上限的增长 min( left_body_len_,MaxSize)
            auto total = left_body_len_;// total_len();
            auto size = buf_.size();
            if (size == MaxSize)
                return;
            
            if (total < MaxSize) {
                if (total > size)
                    resize(total);
            }
            else {
                resize(MaxSize);
            }
        }
        
        //refactor later TODO 
        void expand_size(){ // min( MaxSize,total_len())
                auto total = total_len();
                auto size = buf_.size();
                if (size == MaxSize)
                    return;

                if (total < MaxSize) {
                    if (total > size)
                        resize(total);
                }
                else {
                    resize(MaxSize);
                }
        }
        
        // body_len_ 在 parse_header 里修改
        bool has_body() const {
            return body_len_ != 0 || is_chunked_;
        }

        bool is_http11() {
            return minor_version_ == 1;
        }

        int minor_version() {
            return minor_version_;
        }

        size_t left_body_len() const{ // min(buf_.size(),left_body_len_)
            size_t size = buf_.size();
            return left_body_len_ > size ? size : left_body_len_;
        }

        size_t left_body_size() { // same as up
            //auto size = buf_.size();
            //return left_body_len_ > size ? size : left_body_len_;
            return  left_body_len();
        }

        bool body_finished() {
            return left_body_len_ == 0;
        }

        bool is_chunked() const{
            return is_chunked_;
        }

        bool has_gzip() const {
            return has_gzip_;
        }

        void reduce_left_body_size(size_t size) {
            left_body_len_ -= size;
        }

        void set_current_size(size_t size) {
            cur_size_ = size;
            if (size == 0) {
                copy_method_url_headers();
            }
        }

        //得到header的值,从 copy_headers_ 或 num_headers_
        std::string_view get_header_value(std::string_view key) const {
            if (copy_headers_.empty()) {
                for (size_t i = 0; i < num_headers_; i++) {
                    if (iequal(headers_[i].name, headers_[i].name_len, key.data(), key.length()))
                        return std::string_view(headers_[i].value, headers_[i].value_len);
                }

                return {};
            }

            auto it = std::find_if(copy_headers_.begin(), copy_headers_.end(), [key] (auto& pair){
                if (iequal(pair.first.data(), pair.first.size(), key.data())) {
                    return true;
                }

                return false;
            });

            if (it != copy_headers_.end()) {
                return (*it).second;
            }

            return {};
        }

        // 原始 header 的封装
        std::pair<phr_header*, size_t> get_headers() {
            if(copy_headers_.empty())
                return { headers_ , num_headers_ };

            num_headers_ = copy_headers_.size();
            for (size_t i = 0; i < num_headers_; i++) {
                headers_[i].name      = copy_headers_[i].first.data();
                headers_[i].name_len  = copy_headers_[i].first.size();
                headers_[i].value     = copy_headers_[i].second.data();
                headers_[i].value_len = copy_headers_[i].second.size();
            }
            return { headers_ , num_headers_ };
        }

        //得到multipart_headers_ 中的名字
        //且是从multipart_headers_的开头取的值
        // TODO 为什么只从第一个点开始呢?
        std::string get_multipart_field_name(const std::string& field_name) const {
            if (multipart_headers_.empty())
                return {};

            auto it = multipart_headers_.begin();
            auto val = it->second;
            //auto pos = val.find("name");
            auto pos = val.find(field_name);
            if (pos == std::string::npos) {
                return {};
            }

            auto start = val.find('"', pos) + 1; //这里应该不对 因为Content-Disposition: form-data; name="file"; filename="报名提示.txt"
            auto end = val.rfind('"');
            if (start == std::string::npos || end == std::string::npos || end<start) {
                return {};
            }

            auto key_name = val.substr(start, end - start);
            return key_name;
        }

        //如果原来有值,不会影响原来的值
        void save_multipart_key_value(const std::string& key,const std::string& value)
        {
            if(!key.empty())
            multipart_form_map_.emplace(key,value);
        }

        //增加值的 内容
        void update_multipart_value(std::string key, const char* buf, size_t size) {
            if (!key.empty()) {
                last_multpart_key_ = key;
            }
            else {
                key = last_multpart_key_;
            }

            auto it = multipart_form_map_.find(key);
            if (it != multipart_form_map_.end()) {
                multipart_form_map_[key] += std::string(buf, size);
            }
        }

        std::string get_multipart_value_by_key(const std::string& key)
        {
            if (!key.empty()) {
                return multipart_form_map_[key];
            }

            return {};
        }

        //把 multipart_form_map_ 车 成 form_url_map_
        void handle_multipart_key_value(){
            if(multipart_form_map_.empty()){
                return;
            }

            for (auto& pair : multipart_form_map_) {
                form_url_map_.emplace(std::string_view(pair.first.data(), pair.first.size()), 
                    std::string_view(pair.second.data(), pair.second.size()));
            }
        }

        //是否有 文件
        bool is_multipart_file() const {
            if (multipart_headers_.empty()){
                return false;
            }

            bool has_content_type = (multipart_headers_.find("Content-Type") != multipart_headers_.end());
            auto it = multipart_headers_.find("Content-Disposition");
            bool has_content_disposition = (it != multipart_headers_.end());
            if (has_content_disposition) {
                if (it->second.find("filename") != std::string::npos) {
                    return true;
                }
                return false;
            }

            //最终: "Content-Type" 或 "Content-Disposition" 含有一
            return has_content_type|| has_content_disposition;
        }

        void set_multipart_headers(const multipart_headers& headers) {
            for (auto pair : headers) {
                multipart_headers_[std::string(pair.first.data(), pair.first.size())] = std::string(pair.second.data(), pair.second.size());
            }
        }

        //解析 query
        std::map<std::string_view, std::string_view> parse_query(std::string_view str) {
            std::map<std::string_view, std::string_view> query;
            std::string_view key;
            std::string_view val;
            size_t pos = 0;     //
            size_t length = str.length();
            for (size_t i = 0; i < length; i++) {
                char c = str[i];
                if (c == '=') {
                    key = { &str[pos], i - pos };
                    key = trim(key);
                    pos = i + 1;
                }
                else if (c == '&') {
                    val = { &str[pos], i - pos };
                    val = trim(val);
                    pos = i + 1;
                    //if (is_form_url_encode(key)) {
                    //    auto s = form_urldecode(key);
                    //}
                    query.emplace(key, val);
                }
            }

            if (pos == 0) {
                return {};
            }

            if ((length - pos) > 0) {
                val = { &str[pos], length - pos };
                val = trim(val);
                query.emplace(key, val);
            }
            else if((length - pos) == 0) {
                query.emplace(key, "");
            }

            return query;
        }

        //本质是用 parse_query(解析body)
        bool parse_form_urlencoded() {
            form_url_map_.clear();
#ifdef CINATRA_ENABLE_GZIP //TODO 这里的GZIP uncompress后 parse_query 没有解析 gzip_str_
            if (has_gzip_) {
                bool r = uncompress();
                if (!r)
                    return false;
            }
#endif
            auto body_str = body();
            form_url_map_ = parse_query(body_str);
            if(form_url_map_.empty())
                return false;

            return true;
        }

        //不支持解析
        int parse_chunked(size_t bytes_transferred) {
            auto str = std::string_view(&buf_[header_len_], bytes_transferred - header_len_);

            return -1;
        }

        std::string_view get_method() const{
            if (method_len_ != 0)
                return { method_ , method_len_ };

            return { method_str_.data(), method_str_.length() };
        }

        std::string_view get_url() const {
            if (method_len_ != 0)
                return { url_, url_len_ };

            return { url_str_.data(), url_str_.length() };
        }

        //不带有第一个 slash 符号
        std::string_view get_res_path() const {
            auto url = get_url();

            return url.substr(1);
        }

        // 请求url 当成文件路径
        std::string get_relative_filename() const {
            auto file_name = get_url();
            if (is_form_url_encode(file_name)){
                return code_utils::get_string_by_urldecode(file_name);
            }
            
            return std::string(file_name);
        }

        //去除开头/ 后的地址
        std::string get_filename_from_path() const {
            auto file_name = get_res_path();
            if (is_form_url_encode(file_name)) {
                return code_utils::get_string_by_urldecode(file_name);
            }

            return std::string(file_name.data(), file_name.size());
        }

        std::string_view get_mime(std::string_view filename) const{
            auto extension = get_extension(filename.data());
            auto mime = get_mime_type(extension);
            return mime;
        }

        std::map<std::string_view, std::string_view> get_form_url_map() const{
            return form_url_map_;
        }

        void set_state(data_proc_state state) {
            state_ = state;
        }

        data_proc_state get_state() const{
            return state_;
        }

        void set_part_data(std::string_view data) {
#ifdef CINATRA_ENABLE_GZIP // TODO uncompress 后改变data ???
            if (has_gzip_) {
                bool r = uncompress(data);
                if (!r)
                    return;
            }
#endif
            
            part_data_ = data;
        }

        std::string_view get_part_data() const{
            if (has_gzip_) {
                return { gzip_str_.data(), gzip_str_.length() };
            }

            return part_data_;
        }

        void set_http_type(content_type type) {
            http_type_ = type;
        }

        content_type get_content_type() const {
            return http_type_;
        }

        const std::map<std::string_view, std::string_view>& queries() const{
            return queries_;
        }

        std::string_view get_query_value(size_t n) {
            auto get_val = [&n](auto& map) {
                auto it = map.begin();
                std::advance(it, n);
                return it->second;
            };

            if (n >= queries_.size() ) {
                if(n >= form_url_map_.size())
                    return {};

                return get_val(form_url_map_);
            }
            else {
                return get_val(queries_);
            }
        }

        template<typename T>
        T get_query_value(std::string_view key) {
            static_assert(std::is_arithmetic_v<T>);
            auto val = get_query_value(key);
            if (val.empty()) {
                throw std::logic_error("empty value");
            }

            if constexpr (std::is_same_v<T, int32_t> || std::is_same_v<T, uint32_t> ||
                std::is_same_v<T, bool> || std::is_same_v<T, char> || std::is_same_v<T, short>) {
                int r = std::atoi(val.data());
                if (val[0] != '0' && r == 0) {
                    throw std::invalid_argument(std::string(val) +": is not an integer");
                }
                return r;
            }
            else if constexpr (std::is_same_v<T, int64_t>|| std::is_same_v<T, uint64_t>) {
                auto r = std::atoll(val.data());
                if (val[0] != '0' && r == 0) {
                    throw std::invalid_argument(std::string(val) + ": is not an integer");
                }
                return r;
            }
            else if constexpr (std::is_floating_point_v<T>) {
                char* end;
                auto f = strtof(val.data(), &end);
                if (val.back() != *(end-1)) {
                    throw std::invalid_argument(std::string(val) + ": is not a float");
                }
                return f;
            }
            else {
                throw std::invalid_argument("not support the value type");
            }
        }

        std::string_view get_query_value(std::string_view key){
            auto url = get_url();
            url = url.length()>1 && url.back()=='/' ? url.substr(0,url.length()-1):url;
            std::string map_key = std::string(url.data(),url.size())+std::string(key.data(),key.size());
            auto it = queries_.find(key);
            if (it == queries_.end()) { // 没有从query中找到
                auto itf = form_url_map_.find(key);
                if (itf == form_url_map_.end())
                    return {};

                if(code_utils::is_url_encode(itf->second))
                {
                    auto ret= utf8_character_params_.emplace(map_key, code_utils::get_string_by_urldecode(itf->second)); // TODO ??
                    return std::string_view(ret.first->second.data(), ret.first->second.size());
                }
                return itf->second;
            }
            if(code_utils::is_url_encode(it->second))
            {
                auto ret = utf8_character_params_.emplace(map_key, code_utils::get_string_by_urldecode(it->second));
                return std::string_view(ret.first->second.data(), ret.first->second.size());
            }
            return it->second;
        }

        bool uncompress(std::string_view str) {
            if (str.empty())
                return false;

            bool r = true;
#ifdef CINATRA_ENABLE_GZIP
            gzip_str_.clear();
            r = gzip_codec::uncompress(str, gzip_str_);
#endif
            return r;
        }

        bool uncompress() {
            bool r = true;
#ifdef CINATRA_ENABLE_GZIP
            gzip_str_.clear();
            r = gzip_codec::uncompress(std::string_view(&buf_[header_len_], body_len_), gzip_str_);
#endif
            return r;
        }

        //创建一个file 放到files_ 末尾
        bool open_upload_file(const std::string& filename) {
            upload_file file;
            bool r = file.open(filename);
            if (!r)
                return false;
            
            files_.push_back(std::move(file));
            return true;
        }

        //向files末尾的写东西
        void write_upload_data(const char* data, size_t size) {
            if (size == 0)
                return;

            assert(!files_.empty());

            files_.back().write(data, size);
        }

        //关于 files_末尾的文件
        void close_upload_file() {
            if (files_.empty())
                return;

            files_.back().close();
        }

        //返回files_引用
        const std::vector<upload_file>& get_upload_files() const {
            return files_;
        }

        //得到末尾文件的指针
        upload_file* get_file() {
            if(!files_.empty())
                return &files_.back();

            return nullptr;
        }

        //@desc 得到cookies
        std::map<std::string_view, std::string_view> get_cookies() const
        {
            auto cookies = get_cookies_map(cookie_str_);
            return cookies;
        }

        //@desc 得到cookie 中的值
        std::string_view get_cookie_value() {
            std::string value;
            auto pos= cookie_str_.find(CSESSIONIDWithEQU);
            if( pos == std::string::npos )
                return {}; //空的没有

            auto iter = cookie_str_.begin() + pos + CSESSIONIDWithEQU.length();
            auto point = cookie_str_.data() + pos + CSESSIONIDWithEQU.length();
            std::size_t cnt{0};
            for( ; iter != cookie_str_.end() ; ++iter ){
                if( *iter == ';') break;
                ++cnt;
            }
            return std::string_view{point,cnt};
        }

        void set_range_flag(bool flag)
        {
            is_range_resource_ = flag;
        }

        bool is_range() const
        {
            return is_range_resource_;
        }

        void set_range_start_pos(std::string_view range_header)
        {
            if(is_range_resource_)
            {
               auto l_str_pos = range_header.find("=");
               auto r_str_pos = range_header.rfind("-");
               auto pos_str = range_header.substr(l_str_pos+1,r_str_pos-l_str_pos-1);
               range_start_pos_ = std::atoll(pos_str.data());
            }
        }

        std::int64_t get_range_start_pos() const
        {
            if(is_range_resource_){
                return  range_start_pos_;
            }
            return 0;
        }

        void save_request_static_file_size(std::int64_t size)
        {
            static_resource_file_size_ = size;
        }

        std::int64_t get_request_static_file_size() const
        {
            return static_resource_file_size_;
        }

        void on(data_proc_state event_type, event_call_back&& event_call_back)
        {
            event_call_backs_[(size_t)event_type] = std::move(event_call_back);
        }

        void call_event(data_proc_state event_type) {
            if(event_call_backs_[(size_t)event_type])
            event_call_backs_[(size_t)event_type](*this);
        }

        template<typename... T>
        void set_aspect_data(T&&... data) {
            (aspect_data_.push_back(std::forward<T>(data)), ...);
        }

        void set_aspect_data(std::vector<std::string>&& data) {
            aspect_data_ = std::move(data);
        }

        std::vector<std::string> get_aspect_data() {
            return std::move(aspect_data_);
        }

        void set_last_len(size_t len) {
            last_len_ = len;
        }

        void set_validate(size_t max_header_len, check_header_cb check_headers) {
            max_header_len_ = max_header_len;
            check_headers_ = std::move(check_headers);
        }

        void set_user_id(unsigned long long id){
            m_user_id = id;
        }

        unsigned long long get_user_id() const 
        {
            return m_user_id;
        }

    //private: // TODO
    public:
        void resize_double() {
            size_t size = buf_.size();
            resize(2 * size);
        }

        void resize(size_t size) {
            copy_method_url_headers(); // TODO 为什么 resize_double 的时候要copy? 不copy不节省时间吗?
            buf_.resize(size); //大小指定
        }

        //把 method url headers 转化成string
        void copy_method_url_headers() { //TODO 解析header 结束的时候哪里 copy_method_url_headers ?
            if (method_len_ == 0)
                return;

            method_str_ = std::string( method_, method_len_ );
            url_str_ = std::string(url_, url_len_);
            method_len_ = 0;
            url_len_ = 0;

            auto filename = get_multipart_field_name("filename");
            multipart_headers_.clear();
            if (!filename.empty()) {
                copy_headers_.emplace_back("filename", std::move(filename));
            }            

            if (header_len_ < 0)
                return;

            for (size_t i = 0; i < num_headers_; i++) {
                copy_headers_.emplace_back(std::string(headers_[i].name, headers_[i].name_len), 
                    std::string(headers_[i].value, headers_[i].value_len));
            }
        }

        void check_gzip() {
            auto encoding = get_header_value("content-encoding");
            if (encoding.empty()) {
                has_gzip_ = false;
            }
            else {
                auto it = encoding.find("gzip");
                has_gzip_ = (it != std::string_view::npos);
            }
        }

        constexpr const static size_t MaxSize = __config__::maxRequestSize;
        conn_type conn_;
        response& res_;
        std::vector<char> buf_;

        size_t num_headers_ = 0;
        struct phr_header headers_[32];
        const char *method_ = nullptr;
        size_t method_len_ = 0;
        const char *url_ = nullptr;
        size_t url_len_ = 0;
        int minor_version_ = 0;
        int header_len_ = 0;        //header_占了多少字节长度
        size_t body_len_ = 0;       //body占了多少字节长度

        std::string raw_url_;
        std::string method_str_;
        std::string url_str_;
        std::string cookie_str_;
        std::vector<std::pair<std::string, std::string>> copy_headers_;

        size_t cur_size_ = 0;
        size_t left_body_len_ = 0;

        size_t last_len_ = 0; //for pipeline, last request buffer position

        std::map<std::string_view, std::string_view> queries_; // url 中的 query
        std::map<std::string_view, std::string_view> form_url_map_; //x-www-form-urlencoded
        std::map<std::string,std::string> multipart_form_map_;
        bool has_gzip_ = false;
        std::string gzip_str_;

        bool is_chunked_ = false;

        //validate
        size_t max_header_len_ = 1024 * 1024;
        check_header_cb check_headers_;

        data_proc_state state_  = data_proc_state::data_begin;
        content_type http_type_ = content_type::unknown;
        std::string_view part_data_;

        std::map<std::string, std::string> multipart_headers_;
        std::string last_multpart_key_;
        std::vector<upload_file> files_;
        std::map<std::string,std::string> utf8_character_params_;
        std::map<std::string,std::string> utf8_character_pathinfo_params_;
        std::int64_t range_start_pos_ = 0;
        bool is_range_resource_ = 0;
        std::int64_t static_resource_file_size_ = 0;
        std::vector<std::string> aspect_data_;
        std::array<event_call_back, (size_t)data_proc_state::data_error + 1> event_call_backs_ = {};

        // 一个特别的值,设定这条连接对应的用户的id值(sql的记录id)
        unsigned long long m_user_id = 0;
    };

} // end of namespace netcore
