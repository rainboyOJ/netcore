#pragma once
#include <atomic>
#include <numeric>

// long long 或 int
template<typename T>
struct UUID {
    T get(){
        auto _id = id.fetch_add(1);
        T __max = std::numeric_limits<T>::max();
        id.compare_exchange_strong(__max ,0);
        return _id;
    };
private:
    std::atomic<T> id{0};
};
