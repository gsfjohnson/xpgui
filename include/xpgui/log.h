#pragma once

#include <functional>
#include <string_view>

namespace xpgui {

enum class Level { Debug, Info, Warn, Error };

using LogCallback = std::function<void(Level, std::string_view)>;

// Install a callback that receives all xpgui log output. Replaces any
// previously installed callback. Pass nullptr or call clearLogCallback()
// to drop messages. Thread-safe with respect to the internal helpers.
void setLogCallback(LogCallback cb);

void clearLogCallback();

// Internal helpers used inside xpgui to emit log messages. Calls are no-ops
// when no callback is installed. Not part of the public consumer API, but
// exposed in the public header so other xpgui translation units can use them.
namespace log {
    void debug(std::string_view msg);
    void info(std::string_view msg);
    void warn(std::string_view msg);
    void error(std::string_view msg);
}  // namespace log

}  // namespace xpgui
