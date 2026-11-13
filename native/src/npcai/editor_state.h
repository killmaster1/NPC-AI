#pragma once
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include "types.h"
namespace npcai {
// JNI UI thread produces a completion, game thread consumes it. Empty = cancel.
class EditorState {
    mutable std::mutex mutex_;
    int64_t ticket_=0;
    bool waiting_=false;
    std::optional<std::string> ready_;
public:
    std::optional<int64_t> Begin() {
        std::lock_guard<std::mutex> lock(mutex_);
        if(waiting_ || ready_) return {};
        waiting_=true;return ++ticket_;
    }
    bool Finish(int64_t ticket,std::optional<std::string> text) {
        std::lock_guard<std::mutex> lock(mutex_);
        if(ticket!=ticket_ || !waiting_) return false;
        waiting_=false;
        if(text && (text->size()>1200 || !ValidUtf8(*text))) text.reset();
        ready_=text?std::move(*text):std::string{};
        return true;
    }
    bool Take(std::string& text) {
        std::lock_guard<std::mutex> lock(mutex_);
        if(!ready_) return false;
        text=std::move(*ready_);ready_.reset();return true;
    }
    std::optional<int64_t> Invalidate() {
        std::lock_guard<std::mutex> lock(mutex_);
        if(!waiting_ && !ready_) return {};
        const auto old=ticket_++;waiting_=false;ready_.reset();return old;
    }
    bool Active() const {std::lock_guard<std::mutex> lock(mutex_);return waiting_ || ready_.has_value();}
};
}
