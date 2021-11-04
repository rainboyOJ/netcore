#pragma once

#include <functional>
#include <string_view>
#include <variant>
#include <regex>
#include <type_traits>
#include <httprequest.h>
#include <httpresponse.h>
#include <filesystem>

namespace fs = std::filesystem;

enum class http_method {
    UNKNOW,
	DEL,
	GET,
	HEAD,
	POST, PUT,
	CONNECT,
	OPTIONS,
	TRACE
};
constexpr inline const auto GET     = http_method::GET;
constexpr inline const auto POST    = http_method::POST; constexpr inline const auto DEL     = http_method::DEL; constexpr inline const auto HEAD    = http_method::HEAD;
constexpr inline const auto PUT     = http_method::PUT;
constexpr inline const auto CONNECT = http_method::CONNECT;
constexpr inline const auto TRACE   = http_method::TRACE;
constexpr inline const auto OPTIONS = http_method::OPTIONS;

inline constexpr std::string_view method_name(http_method mthd) {
    using namespace std::literals;
	switch (mthd)
	{
		case http_method::DEL:
			return "DELETE"sv;
			break;
		case http_method::GET:
			return "GET"sv;
			break;
		case http_method::HEAD:
			return "HEAD"sv;
			break;
		case http_method::POST:
			return "POST"sv;
			break;
		case http_method::PUT:
			return "PUT"sv;
			break;
		case http_method::CONNECT:
			return "CONNECT"sv;
			break;
		case http_method::OPTIONS:
			return "OPTIONS"sv;
			break;
		case http_method::TRACE:
			return "TRACE"sv;
			break;
		default:
			return "UNKONWN"sv;
			break;
	}
}

class miniRouter {
    public:
        explicit miniRouter()
            //:HttpServer_{conn}
        {}

    public:
        using routerType = std::function<void(HttpRequest&,HttpResponse&)>;
        using uriType    = std::variant<std::string,std::regex>;

        //路由注册
        template<http_method method = GET,typename Function>
        void reg(uriType uri,Function&& __f){ 
            if constexpr ( method != POST && method != GET){
                throw std::invalid_argument("现在只支持 GET 与 POST");
            }
            routers.push_back( {method_name(method), std::move(uri),std::forward<Function>(__f)} );
        }

        void default_router(HttpRequest& req,HttpResponse& rep) const{
            //TODO 查找相应的文件
            //1.判断是否有后缀
            std::string_view uri_view = req.path();
            if( uri_view[0] == '/') uri_view.remove_prefix(1); // remove slash

            fs::path file_path = uri_view;
            if(!file_path.has_extension())
                file_path  = file_path.string() + ".html";

            //response_.Init(srcDir, request_.path(), request_.IsKeepAlive(), 200);
            std::string path_ = file_path.string();
            rep.Init(srcDir,path_, req.IsKeepAlive(),200);
        }

        // 对所有的路由进行遍历
        void operator()(HttpRequest& req,HttpResponse& rep) const{
            for (const auto& route : routers) {
                if( route.method == req.GetMethod() ){
                    if( std::holds_alternative<std::string>(route.uri) ){
                        if(std::get<std::string>(route.uri) == req.path()) {
                            route.route(req,rep);
                            return;
                        }
                    }
                    else { //regx
                        auto reg = std::get<std::regex>(route.uri);
                        if( std::regex_match(req.path(),req.smatch_,reg) ) //匹配成功
                        {
                            route.route(req,rep);
                            return;
                        }
                    }
                }
            }
            default_router(req, rep); //都没有找到的时候
        }   

        struct node {
            const std::string_view method;
            //const std::string      uri;
            uriType uri;
            routerType  route;
        };

    private:
        std::vector<node> routers;
        const char * srcDir;
        //const HttpServer& HttpServer_; //拥有这个router的服务的引用
};

extern miniRouter Router; //全局变量
