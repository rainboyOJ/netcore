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

#define ok(cond,msg) _ok(cond,__LINE__,msg)

