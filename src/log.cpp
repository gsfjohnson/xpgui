#include "xpgui/log.h"

#include <mutex>
#include <utility>

namespace xpgui {
namespace {

std::mutex& callbackMutex() {
    static std::mutex m;
    return m;
}

LogCallback& callback() {
    static LogCallback cb;
    return cb;
}

void dispatch(Level lvl, std::string_view msg) {
    LogCallback local;
    {
        std::lock_guard<std::mutex> lock(callbackMutex());
        local = callback();
    }
    if (local) local(lvl, msg);
}

}  // namespace

void setLogCallback(LogCallback cb) {
    std::lock_guard<std::mutex> lock(callbackMutex());
    callback() = std::move(cb);
}

void clearLogCallback() {
    std::lock_guard<std::mutex> lock(callbackMutex());
    callback() = nullptr;
}

namespace log {

void debug(std::string_view msg) { dispatch(Level::Debug, msg); }
void info(std::string_view msg)  { dispatch(Level::Info, msg); }
void warn(std::string_view msg)  { dispatch(Level::Warn, msg); }
void error(std::string_view msg) { dispatch(Level::Error, msg); }

}  // namespace log
}  // namespace xpgui
