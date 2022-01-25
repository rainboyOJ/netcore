#pragma once
#include <filesystem>

#ifndef USER_CONFIG
#include "__default_config.hpp"
#endif



namespace fs = std::filesystem;
namespace rojcpp {
    enum class content_type {
        string, // TODO add json
        multipart,
        urlencoded,
        chunked,
        octet_stream,
        websocket,
        unknown,
    };

    enum class req_content_type{
        html,
        json,
        string,
        multipart,
        none
    };

    constexpr inline auto HTML = req_content_type::html;
    constexpr inline auto JSON = req_content_type::json;
    constexpr inline auto TEXT = req_content_type::string;
    constexpr inline auto NONE = req_content_type::none;

    inline const std::string_view STATIC_RESOURCE = "rojcpp_static_resource";
    inline const std::string CSESSIONID = "CSESSIONID";

    const static inline std::string CRCF = "\r\n";
    const static inline std::string TWO_CRCF = "\r\n\r\n";
    const static inline std::string BOUNDARY = "--rojcppBoundary2B8FAF4A80EDB307";
    const static inline std::string MULTIPART_END = CRCF + "--" + BOUNDARY + "--" + TWO_CRCF;


    constexpr static int SESSION_DEFAULT_EXPIRE_{7*24*60*60}; // 7 days

    struct NonSSL {};
    struct SSL {};


}
