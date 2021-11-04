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

void HeapTimer::siftup_(size_t i) {
    assert(i >= 0 && i < heap_.size());
    if(i == 0) return; //如果i是根结点
    size_t j = (i - 1) >> 1; //j中i的父亲
    while(j >= 0) {
        if(heap_[j] < heap_[i]) { break; } //小根堆
        SwapNode_(i, j);
        i = j;
        j = (i - 1) / 2;
    }
}

void HeapTimer::SwapNode_(size_t i, size_t j) {
    assert(i >= 0 && i < heap_.size());
    assert(j >= 0 && j < heap_.size());
    std::swap(heap_[i], heap_[j]);
    ref_[heap_[i].id] = i;
    ref_[heap_[j].id] = j;
} 

bool HeapTimer::siftdown_(size_t index, size_t n) {
    assert(index >= 0 && index < heap_.size());
    assert(n >= 0 && n <= heap_.size());
    size_t i = index;
    size_t j = i * 2 + 1; // 左孩子
    while(j < n) {
        if(j + 1 < n && heap_[j + 1] < heap_[j]) j++; //左右孩子中最小的一个下标
        if(heap_[i] < heap_[j]) break;
        SwapNode_(i, j);
        i = j;
        j = i * 2 + 1;
    }
    return i > index; //是否下降了
}

void HeapTimer::add(int id, int timeout, const TimeoutCallBack& cb) {
    assert(id >= 0);
    size_t i;
    if(ref_.count(id) == 0) {
        /* 新节点：堆尾插入，调整堆 */
        i = heap_.size();
        ref_[id] = i;
        heap_.push_back({id, Clock::now() + MS(timeout), cb});
        siftup_(i);
    } 
    else {
        /* 已有结点：调整堆 */
        i = ref_[id];
        heap_[i].expires = Clock::now() + MS(timeout);
        heap_[i].cb = cb;
        if(!siftdown_(i, heap_.size())) {
            siftup_(i);
        }
    }
}

void HeapTimer::doWork(int id) {
    /* 删除指定id结点，并触发回调函数 */
    if(heap_.empty() || ref_.count(id) == 0) {
        return;
    }
    size_t i = ref_[id];
    TimerNode node = heap_[i];
    node.cb();
    del_(i);
}

void HeapTimer::del_(size_t index) {
    /* 删除指定位置的结点 */
    assert(!heap_.empty() && index >= 0 && index < heap_.size());
    /* 将要删除的结点换到队尾，然后调整堆 */
    size_t i = index;
    size_t n = heap_.size() - 1;
    assert(i <= n);
    if(i < n) {
        SwapNode_(i, n);
        if(!siftdown_(i, n)) {
            siftup_(i);
        }
    }
    /* 队尾元素删除 */
    ref_.erase(heap_.back().id);
    heap_.pop_back();
}

void HeapTimer::adjust(int id, int timeout) {
    /* 调整指定id的结点 */
    assert(!heap_.empty() && ref_.count(id) > 0);
    heap_[ref_[id]].expires = Clock::now() + MS(timeout);;
    siftdown_(ref_[id], heap_.size());
}

void HeapTimer::tick() {
    /* 清除超时结点 */
    if(heap_.empty()) {
        return;
    }
    while(!heap_.empty()) {
        TimerNode node = heap_.front();
        if(std::chrono::duration_cast<MS>(node.expires - Clock::now()).count() > 0) { 
            break; 
        }
        node.cb();
        pop();
    }
}

void HeapTimer::pop() {
    assert(!heap_.empty());
    del_(0);
}

void HeapTimer::clear() {
    ref_.clear();
    heap_.clear();
}

//返回值
int HeapTimer::GetNextTick() { 
    tick();
    size_t res = -1;
    if(!heap_.empty()) {
        res = std::chrono::duration_cast<MS>(heap_.front().expires - Clock::now()).count();
        if(res < 0) { res = 0; }
    }
    return res;
}

} // end namespace rojcpp
#endif //HEAP_TIMER_H
