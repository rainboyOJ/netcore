//lock-free circular array queue
#pragma once
#include <array>
#include <atomic>

template<typename T,size_t size = 100000>
class lockFreeQueue {
public:
    bool queue(T node){
        if( writeableCnt.fetch_sub(1) <= 0){
            writeableCnt.fetch_add(1);
            return false;
        }
        int m = size;
        tail.compare_exchange_strong(m,0); //  因为 tail 只能加1 
        int pos = tail.fetch_add(1) % size; // 得到这个位置

        q[pos] = node; //加入

        readableCnt.fetch_add(1);
        return true;
    }
    bool deque(T& node){

        if( readableCnt.fetch_sub(1) <= 0) {
            readableCnt.fetch_add(1);
            return false;
        }

        int m = size;
        head.compare_exchange_strong(m, 0);
        int pos = head.fetch_add(1) % size;
        node = q[pos];

        writeableCnt.fetch_add(1);

        return true;
    }
private:
    std::array<T, size> q;
    std::atomic_int head{0};
    std::atomic_int tail{0};
    std::atomic_int readableCnt{0};
    std::atomic_int writeableCnt{size};
};
