#include "task.h"

namespace netcore {

    Task::Task(coroutine_handle_type h)
        :m_h{h}
    {}

    Task TaskPromise::get_return_object() {
        m_h = std::coroutine_handle<TaskPromise>::from_promise(*this);
        return { m_h };
    }

    SpawnTask SpawnTaskPromise::get_return_object(){
        return {std::coroutine_handle<SpawnTaskPromise>::from_promise(*this)};
    }
} // end namespace netcore

