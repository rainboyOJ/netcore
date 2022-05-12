//对连接的简单的封装
#pragma once
#include "basic.hpp"
#include "io_context.h"

namespace netcore {

    class Connection {
        private:
            NativeSocket m_socket;
            IoContext * ctx{nullptr};
        public:
            std::size_t read(char * buf,std::size_t buff_size);
            std::size_t send(char * buf,std::size_t buff_size);
        private:
            
    };
    
} // end namespace netcore

