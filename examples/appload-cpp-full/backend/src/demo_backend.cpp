#include "demo_backend.h"

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <string>

#include <fcntl.h>
#include <unistd.h>

namespace {

int g_logFd = -1;

std::string timestamp() {
    const std::time_t t = std::time(nullptr);
    std::tm tm{};
    localtime_r(&t, &tm);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
    return buf;
}

// Logs to stderr (AppLoad forwards the backend's channels) and to a log file.
// AppLoad sets the backend's working directory to the app root, so by default
// the log lands next to the app on the device, e.g.
//   /home/root/xovi/exthome/appload/<app-dir>/backend.log
// Override with the APPLOAD_LOG_FILE env var.
void log(const std::string& msg) {
    const std::string line = "[" + timestamp() + "] cpp-backend: " + msg + "\n";
    std::fwrite(line.data(), 1, line.size(), stderr);
    std::fflush(stderr);
    if (g_logFd >= 0) {
        const ssize_t r = write(g_logFd, line.data(), line.size());
        (void) r;
    }
}

// Crash aid for on-device debugging: append a marker to the log file, then die
// with the usual signal semantics.
void crashHandler(int sig) {
    if (g_logFd >= 0) {
        static const char msg[] = "!!! backend received a fatal signal and is crashing !!!\n";
        const ssize_t r = write(g_logFd, msg, sizeof(msg) - 1);
        (void) r;
        fsync(g_logFd);
    }
    signal(sig, SIG_DFL);
    raise(sig);
}

void reply(const appload::BackendReplier& replier, uint32_t type, const std::string& contents) {
    const bool ok = replier.sendMessage(type, contents);
    log("reply type=" + std::to_string(type) + " length=" + std::to_string(contents.size()) +
        (ok ? "" : " (SEND FAILED)"));
}

} // namespace

DemoBackend::DemoBackend() : startedAt(std::chrono::steady_clock::now()) {
    const char* envPath = std::getenv("APPLOAD_LOG_FILE");
    const std::string path = (envPath && *envPath) ? envPath : "backend.log";
    g_logFd = open(path.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
    for (int sig : {SIGSEGV, SIGABRT, SIGILL, SIGFPE, SIGBUS}) {
        signal(sig, crashHandler);
    }
    log("backend process started, logging to " + path);
}

void DemoBackend::handleMessage(const appload::BackendReplier& replier,
                                const appload::Message& message) {
    log("message type=" + std::to_string(message.msg_type) +
        " length=" + std::to_string(message.contents.size()));

    switch (message.msg_type) {
        case appload::MSG_SYSTEM_NEW_COORDINATOR:
            log("system: a frontend has connected (NEW_COORDINATOR)");
            // Backend-initiated message: broadcast to all frontends.
            reply(replier, demo::MSG_WELCOME, "Hello from the C++ backend!");
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
            // never log full payloads (a huge payload would block the stderr pipe)
            std::string preview = message.contents.substr(0, 200);
            if (message.contents.size() > preview.size()) {
                preview += "...";
            }
            log("echo request (" + std::to_string(message.contents.size()) + " bytes): \"" + preview + "\"");
            reply(replier, demo::MSG_ECHO_REPLY,
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
            reply(replier, demo::MSG_TIME_REPLY, std::string("backend time: ") + buf);
            break;
        }

        case demo::MSG_INCREMENT:
            ++messagesReceived;
            ++counter;
            log("counter incremented to " + std::to_string(counter));
            reply(replier, demo::MSG_COUNTER_REPLY, "counter = " + std::to_string(counter));
            break;

        case demo::MSG_GET_STATS: {
            ++messagesReceived;
            const auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
                                    std::chrono::steady_clock::now() - startedAt)
                                    .count();
            const std::string stats = "uptime=" + std::to_string(uptime) +
                                      "s messages=" + std::to_string(messagesReceived) +
                                      " counter=" + std::to_string(counter);
            reply(replier, demo::MSG_STATS_REPLY, stats);
            break;
        }

        default:
            log("unknown message type " + std::to_string(message.msg_type) + " (payload length " +
                std::to_string(message.contents.size()) + ")");
            break;
    }
}
