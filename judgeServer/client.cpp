#include "client.hpp"
#include <thread>
#include <chrono>

using namespace JUDGESEVER;
int main(){
    judgeClient client("127.0.0.1",8787);
    int i=0;
    while ( 1 ) {
        std::cout << "ping " << ++i << std::endl; 
        client.ping();
        client.dealRecv();
        std::this_thread::sleep_for(std::chrono::seconds(10));
    }
    return 0;
}
