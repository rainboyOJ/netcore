#pragma once

#include <cstdio>
#include <string_view>

namespace boost {
namespace asio {

        using const_buffer = std::string;
        using buffer = std::string;
        //struct const_buffer {
            //const_buffer(const char * ptr,size_t length) 
                //: buf_{ptr,length}
            //{}
            //std::string_view buf_;
        //};

} // end namespace asio
} // end namespace boost


