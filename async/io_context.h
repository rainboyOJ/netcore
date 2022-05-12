// io中心
#pragma once
#include "basic.hpp"

namespace netcore {

    class IoContext;



    class PostTask
    {
    friend IoContext;
    public:
        // your callback
        using callback_type = void (*)(PostTask *);

        void set_callback(callback_type ptr) {
            m_callback = ptr;
        }
        callback_type get_callback() {
            return m_callback;
        }
    private:
        friend class IoCtxBase;
        // use internally
        ListNode m_node;
        callback_type m_callback;

    };

    class IoContext {
    private:
        NativeHandle m_epoll_handle = NULL_HANDLE;
        Queue m_task_queue;
        
        static PostTask *from_node_to_post_task(ListNode *node) {
            //return (PostTask *)((char *)node - offsetof(PostTask, m_node));
            return (PostTask *)((char *)node - offset_of_impl::offsetOf(&PostTask::m_node));
        }

        static ListNode *get_node(PostTask *posttask) {
            return &posttask->m_node;
        }

    public:
        IoContext();
        ~IoContext();
        NativeHandle event_poll_handle() const;
        void post_task(PostTask * callback);
        void run();
    };

    
} // end namespace netcore

