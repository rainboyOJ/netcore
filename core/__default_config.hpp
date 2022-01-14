//全局配置类
//当你需要修改的时候就修改个文件
#pragma once

#include <type_traits>

struct __config__{
    using session_expire = std::integral_constant<int,10>;
};

