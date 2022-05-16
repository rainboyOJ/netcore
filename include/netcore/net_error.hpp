// 所有的异常
#pragma once

#include <execution>

namespace netcore {
   
    struct BaseError :public std::exception {
        std::string m_msg;
        BaseError(std::string&& msg) : m_msg{std::move(msg)} {}
        BaseError(std::string const & msg) : m_msg{msg} {}
        BaseError(const char * msg) : m_msg{msg} {}
        virtual const char * what() const noexcept override {
            return m_msg.c_str();
        }
    };

    
    struct RecvError : public BaseError {
        using BaseError::BaseError;
    };

    struct SendError : public BaseError {
        using BaseError::BaseError;
    };


} // end namespace 

