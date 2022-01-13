#pragma once

template<typename... Args>
void _ok(bool cond,Args&&... args){
    if( cond )
        std::cout << "[OK] ";
    else
        std::cout << "[not OK] ";
    ((std::cout << args << " "),...);
    std::cout << std::endl;
}

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

#define ok(cond,msg) _ok(cond,__LINE__,msg)

