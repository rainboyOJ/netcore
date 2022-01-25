/**
 * 常用的http_sever 使用的工具类函数
 */
#pragma once
#include "connection.hpp"

namespace rojcpp {
/**
 * @brief 得到长度为len的数据
 */
std::string get_send_data(request& req, const size_t len);

//判断一个文件是否是小文件
bool is_small_file(std::ifstream* in,request& req);

//发送一个小文件
void send_small_file(response& res, std::ifstream* in, std::string_view mime);

void write_chunked_header(request& req, std::shared_ptr<std::ifstream> in, std::string_view mime);

void write_chunked_body(request& req);

/**
 * 以ranges方式下载的头部
 */
void write_ranges_header(request& req, std::string_view mime, std::string filename, std::string file_size);

void write_ranges_data(request& req);

void process_download(request& req,response& rep);


} // end namespace rojcpp
