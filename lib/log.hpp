#pragma once
#include <iostream>
#include <mutex>
#include <filesystem>
#include <fstream>
#include <string>
#include <cstring>

namespace  fs = std::filesystem;

extern std::mutex g_log_mutex;  //
template<char Delimiter = ' ',typename... Args>
void debug_out(std::ostream &os, Args&&... args){
    //std::lock_guard<std::mutex> lock(g_log_mutex);
    ( (os << std::dec << args << Delimiter),... ) <<std::endl;
}

template<char Delimiter = '\0',typename... Args>
void debug_out_noendl(std::ostream &os, Args&&... args){
    //std::lock_guard<std::mutex> lock(g_log_mutex);
    ( (os << args << Delimiter),... );
}

#define  log(...) debug_out(std::cout,__FILE__,"Line:",__LINE__,":: ",__VA_ARGS__)
#define log_one(name) log(#name,name)
#define  log_noendl(...) debug_out_noendl(std::cout,__FILE__,"Line:",__LINE__,":: ",__VA_ARGS__)
#define  log_raw(...) debug_out_noendl(std::cout,__VA_ARGS__)
