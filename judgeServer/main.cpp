#include "judgeServer.h"
#include "message.hpp"

using namespace JUDGESEVER;
int main(){
    JudgeServer server(8787);
    server.start();
    return 0;
}
