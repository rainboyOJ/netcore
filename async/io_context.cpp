#include "io_context.h"

namespace netcore {


    IoContext::IoContext(){

        auto fd = epoll_create1(EPOLL_CLOEXEC); //创建一个epoll
        if (fd == -1)
        {
            //TODO better throw
            throw("IoContext().IoContext(): can't create epoll");
        }
        m_epoll_handle = fd;
        log("event poll created", handle_c_str(m_epoll_handle));


        fd = eventfd(1, EFD_NONBLOCK);
        if (fd == -1)
        {
            throw("IoContext().IoContext(): can't create eventfd");
        }

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

    IoContext::~IoContext(){
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
            const int maxevents = 5;
            epoll_event events[maxevents];
            int const timeout = -1; // indefinitely

            //TINYASYNC_LOG("waiting event ... handle = %s", handle_c_str(epfd));
            int nfds = epoll_wait(m_epoll_handle, (epoll_event *)events, maxevents, timeout);

            for (auto i = 0; i < nfds; ++i)
            {
                auto &evt = events[i];
                TINYASYNC_LOG("event %d of %d", i, nfds);
                TINYASYNC_LOG("event = %x (%s)", evt.events, ioe2str(evt).c_str());
                auto callback = (Callback *)evt.data.ptr;
                if (callback >= CallbackGuard)
                {
                    TINYASYNC_LOG("invoke callback");
                    try
                    {
                        callback->callback(evt);
                    }
                    catch (...)
                    {
                        //terminate_with_unhandled_exception();
                        throw("error in IoContext run");
                    }
                }
            }

        } // end for(;;)
    }

} // end namespace netcore

