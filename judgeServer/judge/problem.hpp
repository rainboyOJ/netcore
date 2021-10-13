//得到题目的相关信息
#pragma once
#include <string_view>
#include <exception>
#include <filesystem>
#include <regex>

namespace fs = std::filesystem;

struct Problem {
    Problem()=delete;
    explicit Problem(const std::string_view path,const std::string_view pid)
    {
        //检查路径是否存在
        auto p_path = fs::path(path) / pid;
        //log("p_path",p_path);
        if(not fs::exists(p_path) )
            throw std::runtime_error(p_path.string() + " 不存在!");

        auto data_path = p_path / "data";
        if(not fs::exists(data_path) )
            throw std::runtime_error(data_path.string() + " 不存在!");
        //对数据进行检查

        for ( const auto& e : fs::directory_iterator(data_path) ) {
            auto filename = fs::path(e).filename();
            std::cmatch cm;
            if( std::regex_match(filename.c_str(),cm,input_regex ) ){
                input_data.emplace_back(stoi(cm[1].str()),e.path());
            }
            else if(  std::regex_match(filename.c_str(),cm,output_regex ) ){
                output_data.emplace_back(stoi(cm[1].str()),e.path());
            }
        }
        if( input_data.size() != output_data.size() )
            throw std::runtime_error(p_path.string() + " 输入输出数据个数不匹配");

        sort(input_data.begin(),input_data.end());
        sort(output_data.begin(),output_data.end());
    };

    std::vector<std::pair<int,std::string>> input_data; //数据
    std::vector<std::pair<int,std::string>> output_data; //数据
    const std::regex input_regex{"[A-Za-z_]+(\\d+)\\.in"};
    const std::regex output_regex{"[A-Za-z_]+(\\d+)\\.(out|ans)"};
};
