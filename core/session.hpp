//
// Created by xmh on 18-5-7.
//
#pragma once
#include <iostream>
#include <string>
#include <any>
#include <map>
#include <chrono>
#include <ctime>
#include <mutex>
#include <cstring>
#include <memory>
#include "cookie.hpp"
namespace rojcpp {

    class session
    {
    public:
        session(const std::string& name, const std::string& uuid_str, std::size_t expire, 
            const std::string& path = "/", const std::string& domain = "")
        {
            id_             = uuid_str; //uuid
            expire_         = expire == -1 ? default_expire_ : expire ;//过期 秒
            std::time_t now = std::time(nullptr); //创建时间
            time_stamp_     = expire_ + now;
            cookie_.set_name(name);
            cookie_.set_path(path);
            cookie_.set_domain(domain);
            cookie_.set_value(uuid_str);
            cookie_.set_version(0);
            cookie_.set_max_age(expire == -1 ? -1 : time_stamp_); //过期时间
        }

        //设置内容
        void set_data(const std::string& name, std::any data)
        {
            std::unique_lock<std::mutex> lock(mtx_);
            data_[name] = std::move(data);
        }

        //通过key 得到value
        template<typename T>
        T get_data(const std::string& name)
        {
            std::unique_lock<std::mutex> lock(mtx_);
            auto itert = data_.find(name);
            if (itert != data_.end())
            {
                return std::any_cast<T>(itert->second);
            }
            return T{};
        }

        //是否有 对应的值
        bool has(const std::string& name) {
            std::unique_lock<std::mutex> lock(mtx_);
            return data_.find(name) != data_.end();
        }

        // 本session 的id
        const std::string get_id()
        {
            return id_;
        }

        //设定最大过期
        void set_max_age(const std::time_t seconds)
        {
            std::unique_lock<std::mutex> lock(mtx_);
            is_update_      = true;
            expire_         = seconds == -1 ? 86400 : seconds;
            std::time_t now = std::time(nullptr);
            time_stamp_     = now + expire_;
            cookie_.set_max_age(seconds == -1 ? -1 : time_stamp_);
        }

        //删除
        void remove()
        {
            set_max_age(0);
        }

        rojcpp::cookie& get_cookie()
        {
            return cookie_;
        }

        std::time_t time_stamp() {
            return time_stamp_;
        }

        bool is_need_update()
        {
            std::unique_lock<std::mutex> lock(mtx_);
            return is_update_;
        }

        void set_need_update(bool flag)
        {
            std::unique_lock<std::mutex> lock(mtx_);
            is_update_ = flag;
        }

    private:
        session() = delete;

        std::string id_;
        std::size_t expire_;
        std::time_t time_stamp_;
        std::mutex mtx_;
        cookie cookie_;
        bool is_update_ = true;
        std::map<std::string, std::any> data_; //一个map,存数据
        static constexpr int default_expire_{7*24*60*60};
    };
}
