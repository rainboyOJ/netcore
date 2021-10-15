#include "client.hpp"
#include <thread>
#include <chrono>

using namespace JUDGESEVER;
std::string code = "#include <cstdio>\n int main() { int a,b; scanf(\"%d%d\",&a,&b); printf(\"%d\",a+b); return 0;}";

class myclient : public judgeClient {
    using judgeClient::judgeClient;
    virtual void deal_judge_end(resMessage & rm) override {
        judgeClient::deal_judge_end(rm);
    }
};

int main(){
    Log::Instance()->init_default();
    myclient client("127.0.0.1",8787);
    int i=0;
    //while ( 1 ) {
        //std::cout << "ping " << ++i << std::endl; 
        //client.ping();
        //client.dealRecv();
    client.tryConnect();
    client.startRecvThread();
    client.startSendThread();
    std::this_thread::sleep_for(std::chrono::seconds(6));
    client.sendJudge(1000, 128, 1000, 1, "cpp", code);
    client.join();
        //std::this_thread::sleep_for(std::chrono::seconds(10));
        //client.dealRecv();
    //}

    return 0;
}
