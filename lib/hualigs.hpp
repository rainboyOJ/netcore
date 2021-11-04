//https://www.hualigs.cn/doc/upload
/**
* 遇见图床 api 封装
*
*
*/

#include <string>
#include <string_view>

class Hualigs {
    bool upload(); //上传
private:
    void make_head();
    void make_form_data(); //core
    std::string_view pic_data_;
    std::string  Token_;
};
