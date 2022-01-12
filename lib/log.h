#pragma once
#include <iostream>
#include <mutex>
#include <filesystem>
#include <fstream>
#include <string>
#include <cstring>

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
    void init(int level,                    //等级
            const char* path = "./log",     //路径
            const char* suffix =".log",     //后缀
            int maxQueueCapacity = 1024);   //最大队列

    static Log* Instance();                 //Singleton
    static void FlushLogThread();

    void write(bool appendTitle,int level,bool newline ,const char *format,...); //写
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
    static const int MAX_LINES = 50000; //最大行数

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
    std::unique_ptr<std::thread> writeThread_; //写线程
    std::mutex mtx_;
};

constexpr int32_t basename_index (const char * const path, const int32_t index = 0, const int32_t slash_index = -1)
{
     return path [index]
         ? ( path [index] == '/'
             ? basename_index (path, index + 1, index)
             : basename_index (path, index + 1, slash_index)
           )
         : (slash_index + 1)
     ;
}

#define  __FILENAME__ (__FILE__ + basename_index(__FILE__))
#define LOG_BASE(appendTitle,level, newline,format, ...) \
    do {\
        Log* log = Log::Instance();\
        if (log->IsOpen() && log->GetLevel() <= level) {\
            log->write(appendTitle,level, newline,format, ##__VA_ARGS__); \
            log->flush();\
        }\
    } while(0);



#define LOG_DEBUG(format, ...) do {LOG_BASE(1,0,0,"%s %d ",__FILENAME__,__LINE__);LOG_BASE(0,0,1, format, ##__VA_ARGS__)} while(0);
#define LOG_INFO(format, ...) do {LOG_BASE(1,1,0,"%s %d ",__FILENAME__,__LINE__);LOG_BASE(0,1,1, format, ##__VA_ARGS__)} while(0);
#define LOG_WARN(format, ...) do {LOG_BASE(1,2,0,"%s %d ",__FILENAME__,__LINE__);LOG_BASE(0,2,1, format, ##__VA_ARGS__)} while(0);
#define LOG_ERROR(format, ...) do {LOG_BASE(1,3,0,"%s %d ",__FILENAME__,__LINE__);LOG_BASE(0,3,1, format, ##__VA_ARGS__)} while(0);

#endif //LOG_H
