#include "http_server.h"

using namespace netcore;
int main(){
    http_server hs(1,8899);
    hs.run();
    return 0;
}
