#pragma once

// Wire protocol of AppLoad backends.
// Mirrors src/protocol.h and backends/appload-clients/rust-backend/src/lib.rs
// from https://github.com/asivery/rm-appload

#include <cstdint>

namespace appload {

// Maximum payload size of a single message (10 MiB).
// Mirrors MAX_PACKAGE_SIZE (Rust) / MAX_MESSAGE_LENGTH (C) in rm-appload.
constexpr uint32_t MAX_PACKAGE_SIZE = 10485760;

// System message types (sent by the AppLoad coordinator, not by frontends).
// The values match the signed int defines in src/protocol.h of rm-appload.
constexpr uint32_t MSG_SYSTEM_TERMINATE        = 0xFFFFFFFFu; // -1
constexpr uint32_t MSG_SYSTEM_NEW_COORDINATOR  = 0xFFFFFFFEu; // -2
constexpr uint32_t MSG_SYSTEM_LOST_COORDINATOR = 0xFFFFFFFDu; // -3

// Message header. Because the socket is SOCK_SEQPACKET, the header is sent
// as one packet and the payload as a second packet (two send()/recv() calls),
// exactly like the Rust client does.
#pragma pack(push, 1)
struct MessageHeader {
    uint32_t msg_type;
    uint32_t length; // payload length in bytes; 0 means an empty payload
};
#pragma pack(pop)

static_assert(sizeof(MessageHeader) == 8, "MessageHeader must be exactly 8 bytes");

} // namespace appload
