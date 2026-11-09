#!/usr/bin/env python3
"""Protocol test for the AppLoad C++ backend.

Plays the role of the AppLoad coordinator (mirrors src/management.cpp and
src/protocol.h of https://github.com/asivery/rm-appload):

  * creates an AF_UNIX / SOCK_SEQPACKET listening socket,
  * starts the backend binary with the socket path as argv[1]
    (optionally wrapped in qemu, e.g. --qemu qemu-aarch64, to test the
    aarch64/rMPP binary on an x86_64 machine),
  * accepts the backend connection and checks the whole message exchange:
    header packet (8 bytes: u32 type, u32 length) + payload packet,
    system messages, replies, and clean shutdown on socket close.

Usage:
    python3 tests/protocol_test.py --binary backend/entry
    python3 tests/protocol_test.py --binary backend/entry --qemu qemu-aarch64
"""

import argparse
import os
import socket
import struct
import subprocess
import sys
import tempfile

MSG_SYSTEM_TERMINATE = 0xFFFFFFFF
MSG_SYSTEM_NEW_COORDINATOR = 0xFFFFFFFE
MSG_SYSTEM_LOST_COORDINATOR = 0xFFFFFFFD

# demo message types (see backend/src/demo_backend.h)
MSG_ECHO_REQUEST, MSG_GET_TIME, MSG_INCREMENT, MSG_GET_STATS = 1, 2, 3, 4
MSG_WELCOME, MSG_ECHO_REPLY, MSG_TIME_REPLY, MSG_COUNTER_REPLY, MSG_STATS_REPLY = 100, 101, 102, 103, 104


def check(cond, what):
    if not cond:
        print(f"FAIL: {what}")
        sys.exit(1)
    print(f"ok: {what}")


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--binary", required=True, help="path to the backend binary (entry)")
    ap.add_argument("--qemu", default="", help="qemu wrapper for foreign-arch binaries, e.g. qemu-aarch64")
    ap.add_argument("--socket", default=None, help="socket path (default: temp dir)")
    args = ap.parse_args()

    binary = os.path.abspath(args.binary)
    check(os.path.isfile(binary), f"binary exists: {binary}")

    sock_path = args.socket or os.path.join(tempfile.mkdtemp(prefix="appload-test-"), "test.sock")
    if os.path.exists(sock_path):
        os.unlink(sock_path)

    srv = socket.socket(socket.AF_UNIX, socket.SOCK_SEQPACKET)
    srv.bind(sock_path)
    srv.listen(1)

    cmd = ([args.qemu] if args.qemu else []) + [binary, sock_path]
    print("starting backend:", " ".join(cmd))
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    conn, _ = srv.accept()
    print("backend connected to the coordinator socket")

    def send_msg(msg_type, payload=b""):
        conn.send(struct.pack("<II", msg_type, len(payload)))
        if payload:
            conn.send(payload)

    def recv_msg():
        hdr = conn.recv(8)
        assert len(hdr) == 8, f"bad header packet: {hdr!r}"
        msg_type, length = struct.unpack("<II", hdr)
        if length == 0:
            return msg_type, b""
        data = conn.recv(length)
        assert len(data) == length, f"short payload: {len(data)} != {length}"
        return msg_type, data

    # 1) a frontend connects -> coordinator sends NEW_COORDINATOR,
    #    backend must broadcast a welcome (type 100)
    send_msg(MSG_SYSTEM_NEW_COORDINATOR, b"1")
    t, d = recv_msg()
    check(t == MSG_WELCOME and b"C++ backend" in d, f"NEW_COORDINATOR -> welcome (type {t}: {d!r})")

    # 2) echo request (type 1) -> reply 101
    send_msg(MSG_ECHO_REQUEST, b"Hello")
    t, d = recv_msg()
    check(t == MSG_ECHO_REPLY and b"Hello" in d, f"echo request -> reply (type {t}: {d!r})")

    # 2b) larger payload (100 KiB) through the same framing
    big = os.urandom(100 * 1024)
    send_msg(MSG_ECHO_REQUEST, big)
    t, d = recv_msg()
    check(t == MSG_ECHO_REPLY and big in d, f"100 KiB echo payload round-trip (type {t})")

    # 3) time request (type 2) -> reply 102
    send_msg(MSG_GET_TIME)
    t, d = recv_msg()
    check(t == MSG_TIME_REPLY and b"backend time" in d, f"time request -> reply (type {t}: {d!r})")

    # 4) counter increments (type 3) -> replies 103 with counter = 1, 2
    send_msg(MSG_INCREMENT)
    t, d = recv_msg()
    check(t == MSG_COUNTER_REPLY and b"counter = 1" in d, f"increment -> {d!r}")
    send_msg(MSG_INCREMENT)
    t, d = recv_msg()
    check(t == MSG_COUNTER_REPLY and b"counter = 2" in d, f"increment -> {d!r}")

    # 5) stats (type 4) -> reply 104
    send_msg(MSG_GET_STATS)
    t, d = recv_msg()
    check(t == MSG_STATS_REPLY and b"uptime=" in d and b"counter=2" in d, f"stats -> (type {t}: {d!r})")

    # 6) unknown message type must not kill the backend
    send_msg(99, b"xyz")
    send_msg(MSG_ECHO_REQUEST, b"still alive")
    t, d = recv_msg()
    check(t == MSG_ECHO_REPLY and b"still alive" in d, "unknown type handled, backend still responsive")

    # 7) empty payload (length 0) is valid
    send_msg(MSG_ECHO_REQUEST)
    t, d = recv_msg()
    check(t == MSG_ECHO_REPLY, f"empty payload message ok (type {t})")

    # 8) closing the socket -> backend delivers TERMINATE to itself and exits 0
    conn.close()
    srv.close()
    rc = proc.wait(timeout=15)
    err = proc.stderr.read().decode(errors="replace")
    print("--- backend stderr ---")
    print(err)
    print("-----------------------")
    check(rc == 0, f"backend exited cleanly on socket close (exit code {rc})")
    check("TERMINATE" in err, "backend logged the TERMINATE system message")

    if os.path.exists(sock_path):
        os.unlink(sock_path)
    print("ALL PROTOCOL TESTS PASSED")


if __name__ == "__main__":
    main()
