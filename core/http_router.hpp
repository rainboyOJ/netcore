#pragma once
#include <vector>
#include <map>
#include <unordered_map>
#include <string>
#include <string_view>
#include <regex>    //正则
#include "request.hpp"
#include "response.hpp"
#include "utils.hpp"
#include "function_traits.hpp"
#include "mime_types.hpp"

namespace rojcpp { //路由用的
    namespace{
        constexpr char DOT = '.';
        constexpr char SLASH = '/';
        constexpr std::string_view INDEX = "index";
    }

    class http_router {
    public:
        template<http_method... Is, typename Function, typename... Ap>
        std::enable_if_t<!std::is_member_function_pointer_v<Function>> 
        register_handler(std::string_view name, Function&& f, const Ap&... ap) {
            if constexpr(sizeof...(Is) > 0) {
                auto arr = get_method_arr<Is...>();
                register_nonmember_func(name, arr, std::forward<Function>(f), ap...);
            }
            else {
                register_nonmember_func(name, {0}, std::forward<Function>(f), ap...);
            }
        }

        template<http_method... Is, typename Function, typename... Ap>
        void register_handler_for_regex(std::regex& name,Function&& f,const Ap&... ap){
            if constexpr(sizeof...(Is) > 0) {
                auto arr = get_method_arr<Is...>();
                this->regex_invokers_.emplace_back(name , 
                        { arr, std::bind(&http_router::invoke<Function, Ap...>, this,
                            std::placeholders::_1, std::placeholders::_2, std::move(f), ap...) } 
                        );
            }
            else 
                this->regex_invokers_.emplace_back(name , 
                        { {0}, std::bind(&http_router::invoke<Function, Ap...>, this,
                            std::placeholders::_1, std::placeholders::_2, std::move(f), ap...) } 
                        );
        }

        template <http_method... Is, class T, class Type, typename T1, typename... Ap>
        std::enable_if_t<std::is_same_v<T*, T1>> 
        register_handler(std::string_view name, Type (T::* f)(request&, response&), T1 t, const Ap&... ap) {
            register_handler_impl<Is...>(name, f, t, ap...);
        }

        template <http_method... Is, class T, class Type, typename... Ap>
        void register_handler(std::string_view name, Type(T::* f)(request&, response&), const Ap&... ap) {
            register_handler_impl<Is...>(name, f, (T*)nullptr, ap...);
        }

        void remove_handler(std::string name) {
            this->map_invokers_.erase(name);
        }

        //elimate exception, resut type bool: true, success, false, failed
        bool route(std::string_view method, std::string_view url, request& req, response& res,bool route_it = true) {
            //查询regex类型的 有没有
            for ( int i = 0;i< regex_invokers_.size();i++) {
                auto& r = regex_invokers_[i];
                if(std::regex_match(std::string(url),r.first)){
                    if (method[0] < 'A' || method[0] > 'Z')
                        return false;

                    auto& pair = r.second;
                    if (pair.first[method[0] - 65] == 0) { // 对应的方法没有
                        return false;
                    }
                    if( route_it)
                        pair.second(req,res);
                    return true;
                }
            }

            // 查询完全匹配
            auto it = map_invokers_.find(url); 
            if (it != map_invokers_.end()) {
                auto& pair = it->second;
                if (method[0] < 'A' || method[0] > 'Z')
                    return false;

                if (pair.first[method[0] - 65] == 0) { // 对应的方法没有
                    return false;
                }

                if( route_it )
                    pair.second(req, res);
                return true;
            }
            else {
                if (url.rfind(DOT) != std::string_view::npos) { //是一个文件，包含.字符
                    url = STATIC_RESOURCE;  // 处理静态文件
                    return route(method, url, req, res,route_it); //处理
                }

                return get_wildcard_function(url, req, res,route_it);//模糊匹配
            }
        }

    private:
        bool get_wildcard_function(std::string_view key, request& req, response& res,bool route_it = true) {
            for (auto& pair : wildcard_invokers_) {
                if (key.find(pair.first) != std::string::npos) { //只要能找到一部分
                    auto& t = pair.second;
                    if( route_it )
                        t.second(req, res);
                    return true;
                }
            }
            return false;
        }

        template <http_method... Is, class T, class Type, typename T1, typename... Ap>
        void register_handler_impl(std::string_view name, Type T::* f, T1 t, const Ap&... ap) {
            if constexpr(sizeof...(Is) > 0) {
                auto arr = get_method_arr<Is...>();
                register_member_func(name, arr, f, t, ap...);
            }
            else {
                register_member_func(name, {0}, f, t, ap...);
            }
        }

        /**
         * @brief 注册函数路由的实现
         *
         * @param raw_name url的名称 或 url 本身
         * @param arr 一个表明路由函数注册到哪个http method的数组，列表G 表示GET,P->POST
         * @param f 路由函数类型 @param ap AP路由函数的前置或后置点，就是在路由函数f执行前后执行的函数
         * @return void
         *   @retval void
         */
        template<typename Function, typename... AP>
        void register_nonmember_func(std::string_view raw_name, const std::array<char, 26>& arr, Function f, const AP&... ap) {
            if (raw_name.back()=='*') { //注册到 模糊型容器上
                this->wildcard_invokers_[raw_name.substr(0, raw_name.length()-1)] = { arr, std::bind(&http_router::invoke<Function, AP...>, this,
                    std::placeholders::_1, std::placeholders::_2, std::move(f), ap...) };
            }
            else {//注册到 完全匹配型容器上
                this->map_invokers_[raw_name] = { arr, std::bind(&http_router::invoke<Function, AP...>, this,
                    std::placeholders::_1, std::placeholders::_2, std::move(f), ap...) };
            }
        }

        /**
         * @brief 原始路由函数的执行器，也会执行ap
         */
        template<typename Function, typename... AP>
        void invoke(request& req, response& res, Function f, AP... ap) {
            using result_type = std::result_of_t<Function(request&, response&)>;
            std::tuple<AP...> tp(std::move(ap)...);
            bool r = do_ap_before(req, res, tp); //执行之间的ap

            if (!r)
                return;

            // 如果 路由函数的的返回值类型是void
            if constexpr(std::is_void_v<result_type>) { 
                //business
                f(req, res);
                //after
                do_void_after(req, res, tp);
            }
            else {
                //business
                result_type result = f(req, res);
                //after
                do_after(std::move(result), req, res, tp);
            }
        }

        template<typename Function, typename Self, typename... AP>
        void register_member_func(std::string_view raw_name, const std::array<char, 26>& arr, Function f, Self self, const AP&... ap) {
            if (raw_name.back() == '*') {
                this->wildcard_invokers_[raw_name.substr(0, raw_name.length() - 1)] = { arr, std::bind(&http_router::invoke_mem<Function, Self, AP...>, this,
                    std::placeholders::_1, std::placeholders::_2, f, self, ap...) };
            }
            else {
                this->map_invokers_[raw_name] = { arr, std::bind(&http_router::invoke_mem<Function, Self, AP...>, this,
                    std::placeholders::_1, std::placeholders::_2, f, self, ap...) };
            }
        }

        template<typename Function, typename Self, typename... AP>
        void invoke_mem(request& req, response& res, Function f, Self self, AP... ap) {
            using result_type = typename timax::function_traits<Function>::result_type;
            std::tuple<AP...> tp(std::move(ap)...);
            bool r = do_ap_before(req, res, tp);

            if (!r)
                return;
            using nonpointer_type = std::remove_pointer_t<Self>;
            if constexpr(std::is_void_v<result_type>) {
                //business
                if(self)
                    (*self.*f)(req, res);
                else
                    (nonpointer_type{}.*f)(req, res);
                //after
                do_void_after(req, res, tp);
            }
            else {
                //business
                result_type result;
                if (self)
                    result = (*self.*f)(req, res);
                else
                    result = (nonpointer_type{}.*f)(req, res);
                //after
                do_after(std::move(result), req, res, tp);
            }
        }

        /**
         * @brief 前置ap执行器
         */
        template<typename Tuple>
        bool do_ap_before(request& req, response& res, Tuple& tp) {
            bool r = true;
            for_each_l(tp, [&r, &req, &res](auto& item) {
                if (!r)
                    return;

                constexpr bool has_befor_mtd = has_before<decltype(item), request&, response&>::value;
                if constexpr (has_befor_mtd)
                    r = item.before(req, res);
            }, std::make_index_sequence<std::tuple_size_v<Tuple>>{});

            return r;
        }

        template<typename Tuple>
        void do_void_after(request& req, response& res, Tuple& tp) {
            bool r = true;
            for_each_r(tp, [&r, &req, &res](auto& item) {
                if (!r)
                    return;

                constexpr bool has_after_mtd = has_after<decltype(item), request&, response&>::value;
                if constexpr (has_after_mtd)
                    r = item.after(req, res);
            }, std::make_index_sequence<std::tuple_size_v<Tuple>>{});
        }

        //是否有这个url,考虑到静态文件
        // bool 是否匹配
        // int >=0 匹配的是哪个regex_invokers_的下标
        // -1 匹配的不是regex_invokers_
        // TODO 没有查询 wildcard_invokers_
        auto pick(std::string_view url)-> std::pair<bool,int>
        {
            //查询regex类型的 有没有
            for ( int i = 0;i< regex_invokers_.size();i++) {
                auto& r = regex_invokers_[i];
                if(std::regex_match(std::string(url),r.first)){
                    //TODO method
                    return std::make_pair(true, i);
                }
            }

            auto it = map_invokers_.find(url);
            if( it != map_invokers_.end()){
                auto& pair = it->second;
            }
            return {false,-1};
        }



        template<typename T, typename Tuple>
        void do_after(T&& result, request& req, response& res, Tuple& tp) {
            bool r = true;
            for_each_r(tp, [&r, result = std::move(result), &req, &res](auto& item){
                if (!r)
                    return;

                if constexpr (has_after<decltype(item), T, request&, response&>::value)
                    r = item.after(std::move(result), req, res);
            }, std::make_index_sequence<std::tuple_size_v<Tuple>>{});
        }

        typedef std::pair<std::array<char, 26>, std::function<void(request&, response&)>> invoker_function; //路由函数类型
        std::map<std::string_view, invoker_function> map_invokers_;//型 路由函数容器
        std::unordered_map<std::string_view, invoker_function> wildcard_invokers_; // /url* 型 路由函数容器
        std::vector<std::pair<std::regex,invoker_function>> regex_invokers_; //正则型 路由函数容器
    };
}
