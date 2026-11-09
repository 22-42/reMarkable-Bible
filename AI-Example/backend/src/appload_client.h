#pragma once

// C++ port of the Rust AppLoad backend client
// (backends/appload-clients/rust-backend/src/lib.rs from asivery/rm-appload).
//
// AppLoad starts the backend binary with argv[1] set to the path of a
// temporary unix socket. The backend connects to it (SOCK_SEQPACKET) and
// then receives messages: an 8-byte MessageHeader packet followed by a
// payload packet. Replies use the same framing.

#include "protocol.h"

#include <memory>
#include <string>

namespace appload {

struct Message {
    uint32_t msg_type;
    std::string contents;
};

// Sends replies back to the AppLoad coordinator (which broadcasts them to all
// connected frontends). Mirrors BackendReplier<T> from the Rust client.
class BackendReplier {
public:
    BackendReplier(int fd, std::shared_ptr<bool> locked);

    // Returns false when the frontend side is terminating (mirrors the
    // "locked" replier in the Rust client) or when sending fails.
    bool sendMessage(uint32_t msg_type, const std::string& contents) const;

private:
    int fd;
    std::shared_ptr<bool> locked;
};

// Implement this to build a backend. Mirrors the AppLoadBackend trait.
class AppLoadBackend {
public:
    virtual ~AppLoadBackend() = default;
    virtual void handleMessage(const BackendReplier& replier, const Message& message) = 0;
};

// Mirrors AppLoad<T> from the Rust client: connects to the socket and runs
// the receive loop, dispatching every message to the backend.
class AppLoad {
public:
    // socketPath is the unix socket path passed by AppLoad as argv[1].
    AppLoad(std::unique_ptr<AppLoadBackend> backend, const std::string& socketPath);
    ~AppLoad();

    AppLoad(const AppLoad&) = delete;
    AppLoad& operator=(const AppLoad&) = delete;

    // Runs until the socket is closed, then delivers MSG_SYSTEM_TERMINATE
    // to the backend one last time. Returns 0 on clean shutdown, 1 on error.
    int run();

private:
    int fd;
    std::unique_ptr<AppLoadBackend> backend;
};

} // namespace appload
