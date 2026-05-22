// Smoke test for xpgui log callback wiring. Plain assertions, no test
// framework — xpgui itself has no gtest dependency.

#include "xpgui/log.h"

#include <cassert>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

int main() {
    std::vector<std::pair<xpgui::Level, std::string>> received;

    // No callback installed: helpers must be safe no-ops.
    xpgui::log::info("dropped on the floor");
    assert(received.empty());

    xpgui::setLogCallback([&](xpgui::Level lvl, std::string_view msg) {
        received.emplace_back(lvl, std::string(msg));
    });

    xpgui::log::debug("d");
    xpgui::log::info("i");
    xpgui::log::warn("w");
    xpgui::log::error("e");

    assert(received.size() == 4);
    assert(received[0].first == xpgui::Level::Debug && received[0].second == "d");
    assert(received[1].first == xpgui::Level::Info  && received[1].second == "i");
    assert(received[2].first == xpgui::Level::Warn  && received[2].second == "w");
    assert(received[3].first == xpgui::Level::Error && received[3].second == "e");

    xpgui::clearLogCallback();
    received.clear();
    xpgui::log::info("dropped again");
    assert(received.empty());

    std::puts("xpgui_test_log: ok");
    return 0;
}
