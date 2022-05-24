// io中心
#pragma once
#include "basic.hpp"

namespace netcore {

    std::string ioe2str(epoll_event& evt);

    class IoContext;

    class PostTask
    {
    friend IoContext;
    public:
        // your callback
        using callback_type = void (*)(PostTask *);
        PostTask() = default;
        ~PostTask();

        void set_callback(callback_type ptr);
        callback_type get_callback();
        void call();
        void set_ref_count_ptr(int * _count);
        void set_awaiter_ptr(ListNode * node);
        bool invalid();

        void set_ref_count(int count);
        int get_ref_count() const;
        ListNode * get_m_awater_ptr() const ;

    private:
        int * m_ref_count {nullptr};
        friend class IoCtxBase;
        // use internally
        ListNode m_node;
        ListNode * m_awaiter_ptr{nullptr};
        callback_type m_callback{nullptr};

    };

    class IoContext {
    private:
        NativeHandle m_epoll_handle = NULL_HANDLE;
        Queue m_task_queue;
        PostTask check_expire_awaiter; // 检查过期awaiter
        std::vector<ListNode> m_awaiters; //指向awaiter
        
        static PostTask *from_node_to_post_task(ListNode *node) {
            //return (PostTask *)((char *)node - offsetof(PostTask, m_node));
            return (PostTask *)((char *)node - offset_of_impl::offsetOf(&PostTask::m_node));
        }

        static ListNode *get_node(PostTask *posttask) {
            return &posttask->m_node;
        }


    public:
        IoContext();
        IoContext(NativeHandle epoll_fd); //使用其它 提供的epollfd
        ~IoContext();
        NativeHandle event_poll_handle() const;
        //加入任务
        void post_task(PostTask * callback);
        void push_awaiter(ListNode * node);
        void run();
    };

    
} // end namespace netcore

