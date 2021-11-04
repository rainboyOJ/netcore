//hiredis的cpp 封装
//[redis使用c++ API 的hiredis连接详解_bwangk的博客-CSDN博客](https://blog.csdn.net/bwangk/article/details/83060374)
#pragma once
#include <string>
#include <cstring>
#include <string_view>
#include <vector>
#include <sstream>
#include <iostream>

#include <hiredis/hiredis.h>

namespace REDIS {

using namespace std::string_view_literals;
using namespace std;

class RedisConfig
{

    public:
        RedisConfig(){};
        //获取ip
        std::string_view getRedisIP() const{
            return "127.0.0.1"sv;
        };

        //获取端口号
        int getRedisPort() const{
            return 6379;
        };
};


class RedisTool
{
public:
    RedisTool():m_redis{nullptr} { init(); }
    ~RedisTool(){
        if( m_redis ){ redisFree(m_redis); }
    }

    //向数据库写入string类型数据
    int setString(std::string_view key, std::string_view value){
        if( !m_redis || m_redis->err )//int err; /* Error flags, 错误标识，0表示无错误 */
        {
            cout << "Redis init Error !!!" << endl;
            init();
            return -1;
        }
        redisReply *reply;
        reply = (redisReply *)redisCommand(m_redis,"SET %s %s", key.data(), value.data());//执行写入命令
        cout<<"set string type = "<<reply->type<<endl;//获取响应的枚举类型
        int result = 0;
        if(reply == NULL)
        {
            redisFree(m_redis);
            m_redis = NULL;
            result = -1;
            cout << "set string fail : reply->str = NULL " << endl;
            //pthread_spin_unlock(&m_redis_flock);
            return -1;
        }
        else if(strcmp(reply->str, "OK") == 0)//根据不同的响应类型进行判断获取成功与否
        {
            result = 1;
        }
        else
        {
            result = -1;
            cout << "set string fail :" << reply->str << endl;
        }
        freeReplyObject(reply);//释放响应信息
 
        return result;
    }

    //从数据库读出string类型数据
    std::string getString(std::string_view key){

        if(m_redis == NULL || m_redis->err)
	    {
		    cout << "Redis init Error !!!" << endl;
		    init();
            return NULL;
	    }
	    redisReply *reply;
        reply = (redisReply *)redisCommand(m_redis,"GET %s", key.data());
        cout<<"get string type = "<<reply->type<<endl;

	    if(reply == NULL)
	    {
            redisFree(m_redis);
            m_redis = NULL;
            cout << "ERROR getString: reply = NULL!!!!!!!!!!!! maybe redis server is down" << endl;
            return NULL;
	    }
        else if(reply->len <= 0)
	    {		
            freeReplyObject(reply);
            return NULL;
	    }
	    else
	    {
            stringstream ss;
            ss << reply->str;
            freeReplyObject(reply);
            return ss.str();
        }
    }
 
    int setList(std::string key,std::vector<int> value);
    std::vector<int> getList(std::string key);
 
private:
    void init(){

        struct timeval timeout = { 1, 500000 }; // 1.5 seconds 设置连接等待时间
	    char ip[255];
        strcpy(ip, m_config.getRedisIP().data());
		//cout << "init : ip = " << ip << std::endl;
        m_redis = redisConnectWithTimeout(ip, m_config.getRedisPort(), timeout);//建立连接
        if (m_redis->err) {
            printf("RedisTool : Connection error: %s\n", m_redis->errstr);
        }	
	    else
	    {
            std::cout << "init redis tool success " << std::endl;
            //REDIS_REPLY响应的类型type
            std::cout << "#define REDIS_REPLY_STRING 1"<< std::endl;
            std::cout << "#define REDIS_REPLY_ARRAY 2"<< std::endl;
            std::cout << "#define REDIS_REPLY_INTEGER 3"<< std::endl;
            std::cout << "#define REDIS_REPLY_NIL 4"<< std::endl;
            std::cout << "#define REDIS_REPLY_STATUS 5"<< std::endl;
            std::cout << "#define REDIS_REPLY_ERROR 6"<< std::endl;
        }
    }
    redisContext *m_redis;
    RedisConfig m_config;
    
};

} // end namespace REDIS


