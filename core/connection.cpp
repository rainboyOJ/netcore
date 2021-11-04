#include "connection.hpp"

namespace rojcpp {
        bool HttpConn::isET = true;
        std::atomic<int> HttpConn::userCount = 0;
} // end namespace rojcpp

