#pragma once
#include <iostream>
#include <mutex>
#include <filesystem>
#include <fstream>
#include <string>
#include <cstring>

//extern std::mutex g_log_mutex;  //
//template<char Delimiter = ' ',typename... Args>
//void debug_out(std::ostream &os, Args&&... args){
    ////std::lock_guard<std::mutex> lock(g_log_mutex);
    //( (os << std::dec << args << Delimiter),... ) <<std::endl;
//}

//template<char Delimiter = '\0',typename... Args>
//void debug_out_noendl(std::ostream &os, Args&&... args){
    ////std::lock_guard<std::mutex> lock(g_log_mutex);
    //( (os << args << Delimiter),... );
//}

//#define  log(...) debug_out(std::cout,__FILE__,"Line:",__LINE__,":: ",__VA_ARGS__)
//#define log_one(name) log(#name,name)
//#define log_info(...) log(__VA_ARGS__)
//#define log_error(...) log(__VA_ARGS__)
//#define  log_noendl(...) debug_out_noendl(std::cout,__FILE__,"Line:",__LINE__,":: ",__VA_ARGS__)
//#define  log_raw(...) debug_out_noendl(std::cout,__VA_ARGS__)

/*
 * @Author       : mark
 * @Date         : 2020-06-16
 * @copyleft Apache 2.0
 */ 
#ifndef LOG_H
#define LOG_H

#include <mutex>
#include <string>
#include <thread>
#include <sys/time.h>
#include <string.h>
#include <stdarg.h>           // vastart va_end
#include <assert.h>
#include <sys/stat.h>         //mkdir
#include "blockqueue.h"
#include "buffer.h"

class Log {
public:
    void init_default();   //使用stout作为log
    void init(int level, const char* path = "./log", 
                const char* suffix =".log",
                int maxQueueCapacity = 1024);

    static Log* Instance();
    static void FlushLogThread();

    void write(int level,bool newline ,const char *format,...);
    void flush();

    int GetLevel();
    void SetLevel(int level);
    bool IsOpen() { return isOpen_; }
    
private:
    Log();
    void AppendLogLevelTitle_(int level);
    virtual ~Log();
    void AsyncWrite_();

private:
    static const int LOG_PATH_LEN = 256;
    static const int LOG_NAME_LEN = 256;
    static const int MAX_LINES = 50000;

    const char* path_;
    const char* suffix_;

    int MAX_LINES_;

    int lineCount_;
    int toDay_;

    bool isOpen_;
 
    Buffer buff_;
    int level_;
    bool isAsync_;

    FILE* fp_;
    bool USE_STDOUT;
    std::unique_ptr<BlockDeque<std::string>> deque_; 
    std::unique_ptr<std::thread> writeThread_;
    std::mutex mtx_;
};

#define LOG_BASE(level, newline,format, ...) \
    do {\
        Log* log = Log::Instance();\
        if (log->IsOpen() && log->GetLevel() <= level) {\
            log->write(level, newline,format, ##__VA_ARGS__); \
            log->flush();\
        }\
    } while(0);

#define LOG_DEBUG(format, ...) do {LOG_BASE(0,0,"%s %d ",__FILE__,__LINE__);LOG_BASE(0,1, format, ##__VA_ARGS__)} while(0);
#define LOG_INFO(format, ...) do {LOG_BASE(1,0,"%s %d ",__FILE__,__LINE__);LOG_BASE(1,1, format, ##__VA_ARGS__)} while(0);
#define LOG_WARN(format, ...) do {LOG_BASE(2,0,"%s %d ",__FILE__,__LINE__);LOG_BASE(2,1, format, ##__VA_ARGS__)} while(0);
#define LOG_ERROR(format, ...) do {LOG_BASE(3,0,"%s %d ",__FILE__,__LINE__);LOG_BASE(3,1, format, ##__VA_ARGS__)} while(0);

#endif //LOG_H
