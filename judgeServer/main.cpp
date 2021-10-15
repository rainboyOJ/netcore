#include "judgeServer.h"
#include "message.hpp"

using namespace JUDGESEVER;
int main(){
    Log::Instance()->init_default();
    JudgeServer server(8787,"/home/rainboy/mycode/RainboyOJ/problems/problems/","/tmp");
    server.start();
    return 0;
}
