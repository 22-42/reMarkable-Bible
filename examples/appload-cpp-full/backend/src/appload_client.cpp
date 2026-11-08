#include "appload_client.h"

#include <cerrno>
#include <cstring>
#include <iostream>
#include <vector>

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace appload {

static int connectToSocket(const std::string& path) {
    int fd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if (fd == -1) {
        return -1;
    }

    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    if (path.size() >= sizeof(addr.sun_path)) {
        close(fd);
        errno = ENAMETOOLONG;
        return -1;
    }
    std::strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);

    if (connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        close(fd);
        return -1;
    }
    return fd;
}

BackendReplier::BackendReplier(int fd, std::shared_ptr<bool> locked)
    : fd(fd), locked(std::move(locked)) {}

bool BackendReplier::sendMessage(uint32_t msg_type, const std::string& contents) const {
    if (*locked) {
        // Cannot send back data to a terminating frontend (same as Rust client).
        return false;
    }
    if (contents.size() > MAX_PACKAGE_SIZE) {
        return false;
    }

    const MessageHeader header{msg_type, static_cast<uint32_t>(contents.size())};
    if (send(fd, &header, sizeof(header), 0) == -1) {
        return false;
    }
    if (header.length > 0) {
        if (send(fd, contents.data(), header.length, 0) == -1) {
            return false;
        }
    }
    return true;
}

AppLoad::AppLoad(std::unique_ptr<AppLoadBackend> backend, const std::string& socketPath)
    : fd(connectToSocket(socketPath)), backend(std::move(backend)) {
    if (fd == -1) {
        std::cerr << "appload: failed to connect to socket '" << socketPath
                  << "': " << std::strerror(errno) << std::endl;
    }
}

AppLoad::~AppLoad() {
    if (fd != -1) {
        close(fd);
    }
}

int AppLoad::run() {
    if (fd == -1) {
        return 1;
    }

    MessageHeader header{};
    std::vector<char> buffer(MAX_PACKAGE_SIZE);
    auto locked = std::make_shared<bool>(false);
    BackendReplier replier(fd, locked);

    for (;;) {
        const ssize_t n = recv(fd, &header, sizeof(header), 0);
        if (n < 1) {
            break; // socket closed by the coordinator
        }

        if (header.length > MAX_PACKAGE_SIZE) {
            std::cerr << "appload: message exceeds protocol spec ("
                      << header.length << " > " << MAX_PACKAGE_SIZE << ")" << std::endl;
            return 1;
        }

        std::string contents;
        if (header.length > 0) {
            const ssize_t m = recv(fd, buffer.data(), header.length, 0);
            if (m < 1) {
                std::cerr << "appload: failed to receive payload: "
                          << std::strerror(errno) << std::endl;
                return 1;
            }
            contents.assign(buffer.data(), static_cast<size_t>(m));
        }

        backend->handleMessage(replier, Message{header.msg_type, std::move(contents)});
    }

    // The socket was closed: lock the replier and deliver the terminate
    // message one last time, exactly like the Rust client does.
    *locked = true;
    backend->handleMessage(replier, Message{MSG_SYSTEM_TERMINATE, {}});
    return 0;
}

} // namespace appload
