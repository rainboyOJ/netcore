#include "connection.hpp"

namespace rojcpp {
        std::atomic<int> HttpConn::userCount = 0;

//-------------web socket----------------//
void connection::response_handshake() {
    std::vector<std::string> buffers = res_.to_buffers();
    LOG_DEBUG("response_handshake : %d",buffers.empty());
    std::string buf_to_send;
    if (buffers.empty()) {
        Close(); // TODO 这里要改 我们不主动的关闭自己
        return;
    }
    // 直接写
    for (auto& e : buffers) {
        LOG_DEBUG("%s,%d",e.c_str(),e.length());
        buf_to_send.append(std::move(e));
    }
    LOG_DEBUG("%s",buf_to_send.c_str());
    direct_write(buf_to_send.c_str(), buf_to_send.length());
    //set_continue_workd
    continue_work_ = &connection::ws_response_handshake_continue_work;
}

bool connection::ws_response_handshake_continue_work(){
    req_.set_state(data_proc_state::data_begin); // alias ws_open
    call_back();
    req_.call_event(req_.get_state());
    req_.set_current_size(0);
    //do_read_websocket_head(SHORT_HEADER);
    continue_work_ = &connection::handle_ws_data;
    return TO_EPOLL_READ; //转入读取
}

/**
 * @brief 处理ws数据
 *
 * +->[解析头,解析payload] ---+
 * |                          |
 * +--------------------------+
 *
 *
 */
bool connection::handle_ws_data(){
    auto pointer = req_.data(); // 数据的起始地址
    std::size_t start = 0,end = req_.get_cur_size_();

    std::string ws_body;
    LOG_DEBUG("handle_ws_data, req_.size %d,%d,%d",start,end,end-start);

    do {

        /*---------- ws_header ----------*/
        auto len  = end - start; // TODO
        auto ret  = ws_.parse_header(pointer + start, len);

        if (ret >=   parse_status::complete  /*0*/ ) { //完成了  do_nothing
            start += ret; //更新开始的位置
            len = end - start;
        }
        else if (ret == parse_status::not_complete) {
            return  TO_EPOLL_READ; //再次去读取
        }
        else { //发生错误
            req_.call_event(data_proc_state::data_error);
            Close(); //结束 TODO 应该在哪里结束?
            return TO_EPOLL_WRITE;
        }
        /*---------- ws_header end ----------*/

        /*---------- ws_body ----------*/
        std::string payload;
        ws_frame_type wsft = ws_.parse_payload(pointer + start, len, payload); //解析payload
        if (wsft == ws_frame_type::WS_INCOMPLETE_FRAME) { // 末完成的frame
            // 重新去读取, TODO 把已经处理得到的数据存起来, 显然定义一个新类成员变量 last_ws_str_
            //req_.update_size(bytes_transferred);
            //req_.reduce_left_body_size(bytes_transferred);
            //continue_work_ = &connection::do_read_websocket_data;
            return TO_EPOLL_READ; //继续去读
        }
        if (wsft == ws_frame_type::WS_INCOMPLETE_TEXT_FRAME || wsft == ws_frame_type::WS_INCOMPLETE_BINARY_FRAME) {
            last_ws_str_.append(std::move(payload));
        }

        //进行处理
        //根据结果
        if (!handle_ws_frame(wsft, std::move(payload), 0))
            return TO_EPOLL_WRITE;

    } while ( ws_.is_fin() == false );

    req_.set_current_size(0);
    LOG_DEBUG("after set current_size ,%d",req_.get_cur_size_());
    return TO_EPOLL_READ;
}

/**
 * @brief 读取 websocket_frame 头并处理
 */

bool connection::do_read_websocket_head() {
    size_t length = req_.current_size(); //TODO 读取的长度 ?
    int ret = ws_.parse_header(req_.buffer(), length); //解析头
    //req_.set_current_size(0);

    if (ret == parse_status::complete) { //完成了
        //read payload
        auto payload_length = ws_.payload_length();
        req_.set_body_len(payload_length);
        if (req_.at_capacity(payload_length)) { //超过最大容量
            req_.call_event(data_proc_state::data_error);
            Close(); //结束 TODO 应该在哪里结束?
            return true;
        }
        req_.set_current_size(0); // 测试完了一个frame 不应该清零
        req_.fit_size(); // TODO fit size有什么作用
        continue_work_ = &connection::do_read_websocket_data; 
    }
    else if (ret == parse_status::not_complete) {
        //req_.set_current_size(bytes_transferred); // ?
        return  TO_EPOLL_READ; //再次去读取
        //do_read_websocket_head(ws_.left_header_len());
    }
    else { //发生错误
        req_.call_event(data_proc_state::data_error);
        Close(); //结束 TODO 应该在哪里结束?
    }
    return TO_EPOLL_READ;
}

bool connection::do_read_websocket_data() {

    auto bytes_transferred = 1;
    if (req_.body_finished()) {
        req_.set_current_size(0);
        bytes_transferred = ws_.payload_length(); // TODO 长度应该如何设定?
    }

    std::string payload;
    ws_frame_type ret = ws_.parse_payload(req_.buffer(), bytes_transferred, payload); //解析payload
    if (ret == ws_frame_type::WS_INCOMPLETE_FRAME) { // 末完成的frame
        req_.update_size(bytes_transferred);
        req_.reduce_left_body_size(bytes_transferred);
        continue_work_ = &connection::do_read_websocket_data;
        return TO_EPOLL_READ; //继续去读
    }

    if (ret == ws_frame_type::WS_INCOMPLETE_TEXT_FRAME || ret == ws_frame_type::WS_INCOMPLETE_BINARY_FRAME) {
        last_ws_str_.append(std::move(payload));
    }

    //进行处理
    //根据结果
    if (!handle_ws_frame(ret, std::move(payload), bytes_transferred))
        return TO_EPOLL_WRITE;

    req_.set_current_size(0);
    continue_work_ =  &connection::do_read_websocket_head;
    return TO_EPOLL_READ;
}

// web socket
bool connection::handle_ws_frame(ws_frame_type ret, std::string&& payload, size_t bytes_transferred ) {
    switch (ret)
    {
        case rojcpp::ws_frame_type::WS_ERROR_FRAME:
            req_.call_event(data_proc_state::data_error);
            Close(); /// TODO 关闭
            return false;
        case rojcpp::ws_frame_type::WS_OPENING_FRAME:
            break;
        case rojcpp::ws_frame_type::WS_TEXT_FRAME:
        case rojcpp::ws_frame_type::WS_BINARY_FRAME:
            {
                //reset_timer();
                std::string temp;
                if (!last_ws_str_.empty()) {
                    temp = std::move(last_ws_str_);
                }
                temp.append(std::move(payload));
                req_.set_part_data(temp);
                req_.call_event(data_proc_state::data_continue);
            }
            //on message
            break;
        case rojcpp::ws_frame_type::WS_CLOSE_FRAME:
            {
                close_frame close_frame = ws_.parse_close_payload(payload.data(), payload.length());
                const int MAX_CLOSE_PAYLOAD = 123;
                size_t len = std::min<size_t>(MAX_CLOSE_PAYLOAD, payload.length());
                req_.set_part_data({ close_frame.message, len });
                req_.call_event(data_proc_state::data_close); // 调用

                std::string close_msg = ws_.format_close_payload(opcode::close, close_frame.message, len);
                auto header = ws_.format_header(close_msg.length(), opcode::close);
                send_msg(std::move(header), std::move(close_msg)); //发送
            }
            break;
        case rojcpp::ws_frame_type::WS_PING_FRAME:
            {
                auto header = ws_.format_header(payload.length(), opcode::pong);
                send_msg(std::move(header), std::move(payload));
            }
            break;
        case rojcpp::ws_frame_type::WS_PONG_FRAME:
            ws_ping();
            break;
        default:
            break;
    }

    return true;
}

//定义器
void connection::ws_ping() {
    //timer_.expires_from_now(std::chrono::seconds(60));
    //timer_.async_wait([self = this->shared_from_this()](int const& ec) {
    //if (ec) {
    //self->close(false);
    //return;
    //}

    //self->send_ws_msg("ping", opcode::ping);
    //});
}





} // end namespace rojcpp

