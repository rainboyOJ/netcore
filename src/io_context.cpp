#include "io_context.h"

namespace netcore {


    std::string ioe2str(epoll_event& evt)
    {
        std::string str;
        str += ((evt.events & EPOLLIN) ? "EPOLLIN " : "");;
        str += ((evt.events & EPOLLPRI) ? "EPOLLPRI " : "");
        str += ((evt.events & EPOLLOUT) ? "EPOLLOUT " : "");
        str += ((evt.events & EPOLLRDNORM) ? "EPOLLRDNORM " : "");
        str += ((evt.events & EPOLLRDBAND) ? "EPOLLRDBAND " : "");
        str += ((evt.events & EPOLLWRBAND) ? "EPOLLWRBAND " : "");
        str += ((evt.events & EPOLLMSG) ? "EPOLLMSG " : "");
        str += ((evt.events & EPOLLERR) ? "EPOLLERR " : "");
        str += ((evt.events & EPOLLHUP) ? "EPOLLHUP " : "");
        str += ((evt.events & EPOLLRDHUP) ? "EPOLLRDHUP " : "");
        str += ((evt.events & EPOLLEXCLUSIVE) ? "EPOLLEXCLUSIVE " : "");
        str += ((evt.events & EPOLLWAKEUP) ? "EPOLLWAKEUP " : "");
        str += ((evt.events & EPOLLONESHOT) ? "EPOLLONESHOT " : "");
        str += ((evt.events & EPOLLET) ? "EPOLLET " : "");
        return str;
    }

//======PostTask
    void PostTask::set_callback(callback_type ptr) {
        m_callback = ptr;
    }
    PostTask::callback_type PostTask::get_callback() {
        return m_callback;
    }

    void PostTask::call(){
        if(m_callback !=nullptr) {
            m_callback(this);
        }
    }

    void PostTask::set_ref_count_ptr(int * _count)  { m_ref_count = _count; }
    void PostTask::set_awaiter_ptr(ListNode * node) { m_awaiter_ptr = node; }
    int  PostTask::get_ref_count() const            { return *m_ref_count; }

    bool PostTask::invalid() { return m_callback == nullptr || m_ref_count == nullptr || *m_ref_count <= 0; }

    void PostTask::set_ref_count(int count){
        if( m_ref_count !=nullptr)
            *m_ref_count = count;
        else
            m_ref_count = new int(count);
    }

    PostTask::~PostTask() {
        if( *m_ref_count <= 1) 
            delete m_ref_count;
        else
            --*m_ref_count;
    }

    ListNode * PostTask::get_m_awater_ptr() const {
        return m_awaiter_ptr;
    }

//======IoContext

    IoContext::IoContext(){

        auto fd = epoll_create1(EPOLL_CLOEXEC); //创建一个epoll
        if (fd == -1)
        {
            //TODO better throw
            throw std::runtime_error("IoContext().IoContext(): can't create epoll");
        }
        m_epoll_handle = fd;
        log("event poll created", handle_c_str(m_epoll_handle));


        //fd = eventfd(1, EFD_NONBLOCK);
        //if (fd == -1)
        //{
            //throw("IoContext().IoContext(): can't create eventfd");
        //}

        //m_wakeup_handle = fd;
        //TINYASYNC_LOG("wakeup handle created %s", handle_c_str(m_wakeup_handle));

        //epoll_event evt;
        //evt.data.ptr = (void *)1;
        //evt.events = EPOLLIN | EPOLLONESHOT;
        //if(epoll_ctl(m_epoll_handle, EPOLL_CTL_ADD, m_wakeup_handle, &evt) < 0) {
            //std::string err =  format("can't set wakeup event %s (epoll %s)", handle_c_str(m_wakeup_handle), handle_c_str(m_epoll_handle));
            //TINYASYNC_LOG(err.c_str());
            ////throw_errno(err);
            //throw(err);
        //}

    }
    IoContext::IoContext(NativeHandle epoll_fd)
        :m_epoll_handle(epoll_fd)
    {
        log("m_epoll_handle",epoll_fd);
        //log("event poll created", handle_c_str(m_epoll_handle));
    }

    IoContext::~IoContext(){
        log("===== ~IoContext");
        ::close(m_epoll_handle);
    }

    void IoContext::post_task(PostTask * callback){
        m_task_queue.push(get_node(callback));
    }

    NativeHandle IoContext::event_poll_handle() const
    {
        return  m_epoll_handle;
    }

    void IoContext::run() {
        Callback *const CallbackGuard = (Callback *)8;
        for(;;) {
            // ===== 进行 注册任务的检查
            //遍历任务
            log(">>>>> check m_task_queue ");
            ListNode * end = m_task_queue.m_tail;
            ListNode * node_ptr  = m_task_queue.pop();

            for( ; node_ptr != end ; node_ptr = m_task_queue.pop() ){
                //转成
                log("node_ptr",node_ptr,"end",end);
                auto post_task = from_node_to_post_task(node_ptr);
                try  {
                    //post_task->get_callback()(post_task);
                    post_task->call(); //执行
                }
                catch(...){
                    log("catch io_context post_task error!");
                }
            }
            if( end !=nullptr) {
                log("call end ptr:",end);
                try  {
                    auto post_task = from_node_to_post_task(end);
                    post_task->call(); //执行
                }
                catch(...){
                    log("catch io_context post_task error!");
                }
            }


            // ===== 进行epoll event的检查
            const int maxevents = 5;
            epoll_event events[maxevents];
            int const timeout = 0; // indefinitely

            //TINYASYNC_LOG("waiting event ... handle = %s", handle_c_str(epfd));
            int nfds = epoll_wait(m_epoll_handle, (epoll_event *)events, maxevents, timeout);

            //log( "epoll_wait nfds:",nfds);
            for (auto i = 0; i < nfds; ++i)
            {
                auto &evt = events[i];
                log("event fd",evt.data.fd," events: ",ioe2str(evt));
                TINYASYNC_LOG("event %d of %d", i, nfds);
                TINYASYNC_LOG("event = %x (%s)", evt.events, ioe2str(evt).c_str());
                auto callback = (Callback *)evt.data.ptr;
                log("invoke callback");
                callback->callback(evt);
                //if (callback >= CallbackGuard)
                //{
                    //TINYASYNC_LOG("invoke callback");
                    //try
                    //{
                    //}
                    //catch (...)
                    //{
                        ////terminate_with_unhandled_exception();
                        //throw std::runtime_error("error in IoContext run");
                    //}
                //}
            }

        } // end for(;;)
    }

} // end namespace netcore

