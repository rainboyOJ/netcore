/*
 * @Author       : mark
 * @Date         : 2020-06-17
 * @copyleft Apache 2.0
 * @update by rainboy 2021-11-02
 */ 
#ifndef HEAP_TIMER_H
#define HEAP_TIMER_H
#pragma once

#include <queue>
#include <unordered_map>
#include <time.h>
#include <algorithm>
#include <arpa/inet.h> 
#include <functional> 
#include <assert.h> 
#include <chrono>
#include "log.h"

namespace rojcpp {
    

typedef std::function<void()> TimeoutCallBack; //超时回调
typedef std::chrono::high_resolution_clock Clock; //时钟
typedef std::chrono::milliseconds MS; // 时间
typedef Clock::time_point TimeStamp; //时间戳

struct TimerNode {
    int id;
    TimeStamp expires;
    TimeoutCallBack cb;
    bool operator<(const TimerNode& t) { //比较
        return expires < t.expires;
    }
};
class HeapTimer {
public:
    HeapTimer() { heap_.reserve(64); }
    ~HeapTimer() { clear(); }
    void adjust(int id, int newExpires); //调整
    void add(int id, int timeOut, const TimeoutCallBack& cb); //加入
    void doWork(int id); //工作
    void clear();
    void tick(); //
    void pop();
    int GetNextTick();
private:
    void del_(size_t i);
    void siftup_(size_t i);
    bool siftdown_(size_t index, size_t n);
    void SwapNode_(size_t i, size_t j);
    std::vector<TimerNode> heap_{};
    std::unordered_map<int, size_t> ref_;
    std::mutex mutex_;
};


} // end namespace rojcpp
#endif //HEAP_TIMER_H
