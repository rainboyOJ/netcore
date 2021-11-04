/**
* sm.ms 作为图床 的类封装
* 发现sm.ms作为图床有限制，5GB空间 100per limit /day 
*
*/
#include <string>
#include <string_view>

class SMMS {
    bool upload(); //上传
    private:
    void make_form_data();
    std::string_view pic;
    std::string  Token;
};
