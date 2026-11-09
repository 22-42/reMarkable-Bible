#pragma once

// The demo backend logic: an extended version of
// examples/appload/full/backend/src/main.rs from asivery/rm-appload.

#include "appload_client.h"

#include <chrono>

// Message types used between the QML frontend and this backend.
// System types (0xFFFFFFFD..0xFFFFFFFF) live in protocol.h.
namespace demo {
constexpr uint32_t MSG_ECHO_REQUEST  = 1;   // frontend -> backend: text to echo back
constexpr uint32_t MSG_GET_TIME      = 2;   // frontend -> backend: empty payload
constexpr uint32_t MSG_INCREMENT     = 3;   // frontend -> backend: empty payload
constexpr uint32_t MSG_GET_STATS     = 4;   // frontend -> backend: empty payload

constexpr uint32_t MSG_WELCOME       = 100; // backend -> frontend, sent on NEW_COORDINATOR
constexpr uint32_t MSG_ECHO_REPLY    = 101; // backend -> frontend
constexpr uint32_t MSG_TIME_REPLY    = 102; // backend -> frontend
constexpr uint32_t MSG_COUNTER_REPLY = 103; // backend -> frontend
constexpr uint32_t MSG_STATS_REPLY   = 104; // backend -> frontend
} // namespace demo

class DemoBackend : public appload::AppLoadBackend {
public:
    DemoBackend();

    void handleMessage(const appload::BackendReplier& replier,
                       const appload::Message& message) override;

private:
    std::chrono::steady_clock::time_point startedAt;
    unsigned long messagesReceived = 0;
    long counter = 0;
};
