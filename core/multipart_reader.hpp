#ifndef _MULTIPART_READER_H_
#define _MULTIPART_READER_H_

#include <map>
#include <utility>
#include <string>
#include <functional>
#include "multipart_parser.hpp"

namespace rojcpp{
using multipart_headers = std::multimap<std::string, std::string>; // TODO 为什么用multimap ??

class multipart_reader {
public:
    using PartBeginCallback = std::function<void(const multipart_headers &)>;
    using PartDataCallback = std::function<void(const char*, size_t)>;
    using PartDataEnd = std::function<void()>;
    //typedef void (*PartBeginCallback)(const multipart_headers &headers);
    //typedef void (*PartDataCallback)(const char *buffer, size_t size);
    //typedef void (*Callback)();

    // 外部赋值，直接调用
    PartBeginCallback on_part_begin;  // part 的header读取完了数据刚开始
    PartDataCallback  on_part_data;   // part 的数据读取完了
    PartDataEnd       on_part_end;    // part 结束了
    PartDataEnd       on_end;         // 全部结束了
    
    multipart_reader() {
        resetReaderCallbacks();
        setParserCallbacks();
    }
    
    void reset() {
        parser.reset();
    }
    
    void set_boundary(std::string&& boundary) {
        parser.set_boundary(std::move(boundary)); //should add \r\n-- ?
    }
    
    size_t feed(const char *buffer, size_t len) {
        return parser.feed(buffer, len);
    }
    
    bool succeeded() const {
        return parser.succeeded();
    }
    
    bool has_error() const {
        return parser.has_error();
    }
    
    bool stopped() const {
        return parser.stopped();
    }
    
    const char *get_error_message() const {
        return parser.get_error_message();
    }

private:
    void resetReaderCallbacks() {
        on_part_begin = nullptr;
        on_part_data = nullptr;
        on_part_end = nullptr;
        on_end = nullptr;
    }

    void setParserCallbacks() { //注册这些cb
        parser.onPartBegin = cbPartBegin;
        parser.onHeaderField = cbHeaderField;
        parser.onHeaderValue = cbHeaderValue;
        parser.onHeaderEnd = cbHeaderEnd;
        parser.onHeadersEnd = cbHeadersEnd;
        parser.onPartData = cbPartData;
        parser.onPartEnd = cbPartEnd;
        parser.onEnd = cbEnd;
        parser.userData = this; //指向当前的reader
    }

    static void cbPartBegin(const char *buffer, size_t start, size_t end, void *userData) {
        //multipart_reader *self = (multipart_reader *)userData;
        //self->currentHeaders.clear();
    }

    static void cbHeaderField(const char *buffer, size_t start, size_t end, void *userData) {
        multipart_reader *self = (multipart_reader *)userData;
        self->currentHeaderName += { buffer + start, end - start };
    }

    static void cbHeaderValue(const char *buffer, size_t start, size_t end, void *userData) {
        multipart_reader *self = (multipart_reader *)userData;
        self->currentHeaderValue += { buffer + start, end - start };
    }

    //解析一个header 结束的时候
    static void cbHeaderEnd(const char *buffer, size_t start, size_t end, void *userData) {
        multipart_reader *self = (multipart_reader *)userData;
        self->currentHeaders.emplace(self->currentHeaderName, self->currentHeaderValue);
        self->currentHeaderName.clear();
        self->currentHeaderValue.clear();
        //self->currentHeaders.emplace(std::string{ self->currentHeaderName.data(), self->currentHeaderName.length() },
        //    std::string{ self->currentHeaderValue.data(), self->currentHeaderValue.length() });
    }

    // cbHeadersEnd multipart_里的header解析完的时候
    static void cbHeadersEnd(const char *buffer, size_t start, size_t end, void *userData) {
        multipart_reader *self = (multipart_reader *)userData;
        if (self->on_part_begin != nullptr) {
            self->on_part_begin(self->currentHeaders);
        }
        self->currentHeaders.clear(); //清空所有的headers
    }

    static void cbPartData(const char *buffer, size_t start, size_t end, void *userData) {
        multipart_reader *self = (multipart_reader *)userData;
        if (self->on_part_data != nullptr) {
            self->on_part_data(buffer + start, end - start); //解析数据时候的动作
        }
    }

    static void cbPartEnd(const char *buffer, size_t start, size_t end, void *userData) {
        multipart_reader *self = (multipart_reader *)userData;
        if (self->on_part_end != nullptr) {
            self->on_part_end(); // 一个part结束时候的动作
        }
    }

    static void cbEnd(const char *buffer, size_t start, size_t end, void *userData) {
        multipart_reader *self = (multipart_reader *)userData;
        if (self->on_end != nullptr) {
            self->on_end();
        }
    }

private:
    multipart_parser parser;
    multipart_headers currentHeaders;
    std::string currentHeaderName, currentHeaderValue;
    void *userData;
};
}
#endif /* _MULTIPART_READER_H_ */
