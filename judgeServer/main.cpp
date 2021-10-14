#include "judgeServer.h"
#include "message.hpp"

using namespace JUDGESEVER;
int main(){
    JudgeServer server(8787,"/home/rainboy/mycode/RainboyOJ/problems/problems/","/tmp");
    server.start();
    return 0;
}
