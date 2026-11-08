#include "demo_backend.h"

#include <cstdio>
#include <ctime>
#include <string>

namespace {

std::string timestamp() {
    const std::time_t t = std::time(nullptr);
    std::tm tm{};
    localtime_r(&t, &tm);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
    return buf;
}

void log(const std::string& msg) {
    std::fprintf(stderr, "[%s] cpp-backend: %s\n", timestamp().c_str(), msg.c_str());
    std::fflush(stderr);
}

} // namespace

DemoBackend::DemoBackend() : startedAt(std::chrono::steady_clock::now()) {
    log("backend process started");
}

void DemoBackend::handleMessage(const appload::BackendReplier& replier,
                                const appload::Message& message) {
    switch (message.msg_type) {
        case appload::MSG_SYSTEM_NEW_COORDINATOR:
            log("system: a frontend has connected (NEW_COORDINATOR)");
            // Backend-initiated message: broadcast to all frontends.
            replier.sendMessage(demo::MSG_WELCOME, "Hello from the C++ backend!");
            break;

        case appload::MSG_SYSTEM_LOST_COORDINATOR:
            log("system: a frontend disconnected (LOST_COORDINATOR)");
            break;

        case appload::MSG_SYSTEM_TERMINATE:
            log("system: TERMINATE received, shutting down after " +
                std::to_string(messagesReceived) + " handled message(s)");
            break;

        case demo::MSG_ECHO_REQUEST: {
            ++messagesReceived;
            log("echo request: \"" + message.contents + "\"");
            replier.sendMessage(demo::MSG_ECHO_REPLY,
                                "Backend responds to message " + message.contents);
            break;
        }

        case demo::MSG_GET_TIME: {
            ++messagesReceived;
            const std::time_t t = std::time(nullptr);
            std::tm tm{};
            localtime_r(&t, &tm);
            char buf[32];
            std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
            replier.sendMessage(demo::MSG_TIME_REPLY, std::string("backend time: ") + buf);
            break;
        }

        case demo::MSG_INCREMENT:
            ++messagesReceived;
            ++counter;
            log("counter incremented to " + std::to_string(counter));
            replier.sendMessage(demo::MSG_COUNTER_REPLY,
                                "counter = " + std::to_string(counter));
            break;

        case demo::MSG_GET_STATS: {
            ++messagesReceived;
            const auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
                                    std::chrono::steady_clock::now() - startedAt)
                                    .count();
            const std::string stats = "uptime=" + std::to_string(uptime) +
                                      "s messages=" + std::to_string(messagesReceived) +
                                      " counter=" + std::to_string(counter);
            replier.sendMessage(demo::MSG_STATS_REPLY, stats);
            break;
        }

        default:
            log("unknown message type " + std::to_string(message.msg_type) + " (payload length " +
                std::to_string(message.contents.size()) + ")");
            break;
    }
}
