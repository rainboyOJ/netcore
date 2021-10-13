#include "message.hpp"

using namespace JUDGESEVER;
int main(){
    judgeMessage a(1000,128,1,"cpp","code 1231 \n");
    a.debug();
    std::cout << a.encode() << std::endl;
    auto str = a.encode();
    judgeMessage b;
    for (const auto& e : str) {
        std::cout << static_cast<int>(e) << " " ;
        b.decode(e);
    }
    b.debug();
    return 0;
}
