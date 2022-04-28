//@desc 对easyloggingpp 进行初始化
#pragma once
#include "easyloggingpp/easylogging++.h"

#define ELPP_THREAD_SAFE //线程安全

INITIALIZE_EASYLOGGINGPP

namespace netcore {
    
//初始化配置

class InitLog {
public:
    static InitLog & start() {
        static InitLog __init;
        return __init;
    }
private:
    InitLog(){

        el::Loggers::addFlag(el::LoggingFlag::DisableApplicationAbortOnFatalLog);

        el::Configurations defaultConf;
        defaultConf.setToDefault();
#ifdef DEBUG
        el::Loggers::addFlag(el::LoggingFlag::ColoredTerminalOutput);
        //不需要输出到 文件
        defaultConf.setGlobally(el::ConfigurationType::ToFile, "false");
#else
        //不需要输出到 terminal
        defaultConf.setGlobally(el::ConfigurationType:ToStandardOutput:,  "false");
        defaultConf.setGlobally(el::configurationType::MaxLogFileSize,    "2097152"); // 2mb
        defaultConf.setGlobally(el::configurationType::LogFlushThreshold, "100"); // 2mb{
        //关闭不需要的LOG
        defaultConf.set(el::Level::Info,  el::ConfigurationType::Enabled, "false");
        defaultConf.set(el::Level::Trace, el::ConfigurationType::Enabled, "false");
        defaultConf.set(el::Level::Debug, el::ConfigurationType::Enabled, "false");
    }
#endif
        el::Loggers::reconfigureLogger("default", defaultConf);
    }
};

static auto & ______very_long____name__and_not_to_use = InitLog::start();

} // end namespace netcore
