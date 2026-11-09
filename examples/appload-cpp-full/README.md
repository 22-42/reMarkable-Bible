# AppLoad example app with QML frontend + C++ backend

A complete example [AppLoad](https://github.com/asivery/rm-appload) application for
reMarkable tablets (XOVI extension), consisting of a QML frontend and a **C++ backend**
that is a port of the Rust backend client
(`backends/appload-clients/rust-backend`) from `asivery/rm-appload`.

The demo extends `examples/appload/full` of rm-appload: the frontend can send four
different message types, the backend keeps state (counter, stats) and also pushes a
welcome message to frontends when they connect.

## Directory layout (source)

```
appload-cpp-full/
|-> manifest.json          # app metadata (id, name, entry, ...)
|-> icon.png.b64           # launcher icon, base64-encoded (decoded by the build scripts; drop a real icon.png next to them to override)
|-> application.qrc        # Qt resource file listing the QML files
|-> ui/
|   |-> example.qml        # root QML file (manifest "entry": /ui/example.qml)
|-> backend/
|   |-> Makefile           # native / armv7 / aarch64 builds of the backend
|   |-> src/
|   |   |-> protocol.h         # wire protocol constants (mirrors src/protocol.h of rm-appload)
|   |   |-> appload_client.*   # C++ port of the Rust AppLoad/BackendReplier client
|   |   |-> demo_backend.*     # the demo message handlers (like full/backend/src/main.rs)
|   |   |-> main.cpp           # entry point: AppLoad app(make_unique<DemoBackend>(), argv[1]).run()
|-> build-pc.sh            # build for the PC emulator   -> ./output
|-> build-rm.sh            # build for rM1/rM2 (armv7)   -> ./output-rm
|-> build-rmpp.sh          # build for rMPP (aarch64)    -> ./output-rmpp
```

## Build output layout (what AppLoad expects on the device/emulator)

```
output*/
|-> manifest.json
|-> icon.png
|-> resources.rcc          # compiled with: rcc --binary -o resources.rcc application.qrc
|-> backend/
    |-> entry              # the compiled C++ backend binary (executable)
```

Copy the whole `output*` directory to:

- **Tablet**: `/home/root/xovi/exthome/appload/<any-name>/` (the directory name does not matter)
- **PC emulator**: the `applications_root` folder of the emulator

## Building

Prerequisites: `rcc` (Qt), `make`, a C++17 compiler. For the tablet builds you also
need the cross toolchains `arm-linux-gnueabihf-g++` (rM1/rM2) and
`aarch64-linux-gnu-g++` (rMPP).

```bash
./build-pc.sh     # PC emulator (native)
./build-rm.sh     # reMarkable 1 / 2
./build-rmpp.sh   # reMarkable Paper Pro
```

The backend alone can also be built by hand:

```bash
make -C backend          # native  -> backend/entry
make -C backend rm       # armv7   -> backend/entry (fully static)
make -C backend rmpp     # aarch64 -> backend/entry (fully static)
```

The cross builds (`rm`, `rmpp`) are linked **fully static** (`-static`), so the
binary runs on the tablet regardless of the device's glibc version. A dynamically
linked cross build can fail on the device with `GLIBC_x.yy not found` — the backend
would then never start and the frontend would show no backend messages at all.

## Testing

### Backend protocol test (local or CI)

`tests/protocol_test.py` plays the role of the AppLoad coordinator (mirrors
`src/management.cpp` / `src/protocol.h` of rm-appload): it creates a `SOCK_SEQPACKET`
unix socket, starts the backend with the socket path as `argv[1]` and checks the whole
message exchange (system messages, echo incl. a 100 KiB payload, counter, stats,
clean shutdown on socket close).

```bash
make -C backend all
python3 tests/protocol_test.py --binary backend/entry                      # native (PC)
# rMPP binary on an x86_64 machine (needs qemu-user):
python3 tests/protocol_test.py --binary backend/entry --qemu qemu-aarch64
```

### End-to-end in the AppLoad PC emulator

The CI job `emulator-e2e` (see `.github/workflows/build-appload-cpp-full.yml`) builds the
rm-appload PC emulator (`qmake6 appload.pro && make`), installs the PC build of this app
into `applications_root/` and runs `xvfb-run ./appload cpp-backend-example` headless — the
emulator autostarts the app passed as `argv[1]` (see `src/EmuOnly.h` of rm-appload). The job
asserts that the backend was started, received `NEW_COORDINATOR`, and that its welcome
message (type 100) arrived in the QML frontend (`onMessageReceived` -> `console.log`).

Locally on Linux:

```bash
git clone https://github.com/asivery/rm-appload.git && cd rm-appload
qmake6 appload.pro && make
mkdir -p applications_root
cp -r <this repo>/examples/appload-cpp-full/output applications_root/appload-cpp-full
./appload cpp-backend-example     # argv[1] = app id from manifest.json -> autostart
```

### Diagnosing on the tablet

The backend's stdout/stderr are forwarded by AppLoad (`QProcess::ForwardedChannels`),
so they end up in the journal. If the frontend shows but the backend does not answer:

```bash
APP=/home/root/xovi/exthome/appload/appload-cpp-full
ls -la "$APP" "$APP/backend"
"$APP/backend/entry"; echo "exit=$?"     # no args -> usage, exit 2; loader error -> glibc mismatch
ldd --version | head -1                   # glibc version of the tablet
ldd "$APP/backend/entry" 2>&1 | head
journalctl -b --no-pager | grep -iE 'appload|cpp-backend|Starting binary|No active socket|Failed to start' | tail -60
```

## CI build (GitHub Actions)

The workflow [`.github/workflows/build-appload-cpp-full.yml`](../../.github/workflows/build-appload-cpp-full.yml)
builds the complete app for the **rMPP (aarch64)** — frontend (`resources.rcc` via `rcc`) and
C++ backend — on every push/PR that touches this directory, and manually via "Run workflow"
in the Actions tab. Download the `appload-cpp-full-rmpp` artifact from the workflow run and
copy the contained `output-rmpp/` directory to the tablet:

```bash
scp -r output-rmpp/ root@<tablet-ip>:/home/root/xovi/exthome/appload/appload-cpp-full/
```

## The AppLoad backend protocol (implemented in `backend/src/`)

- AppLoad starts the backend binary with **argv[1] = path of a temporary unix socket**.
- The backend connects with `socket(AF_UNIX, SOCK_SEQPACKET, 0)` + `connect()`.
- Each message is framed as **two packets**: an 8-byte header
  `struct { uint32_t msg_type; uint32_t length; }` (see `protocol.h`, mirrors
  `PacketHeader { int type; int messageLength; }` in `src/protocol.h` of rm-appload),
  followed by `length` bytes of payload. Maximum payload: **10 MiB** (`length == 0`
  means an empty payload).
- Replies use the same framing and are broadcast to all connected frontends.
- System message types: `0xFFFFFFFF` TERMINATE (delivered once more when the socket
  closes, then the backend should exit), `0xFFFFFFFE` NEW_COORDINATOR (a frontend
  connected), `0xFFFFFFFD` LOST_COORDINATOR.

### Demo message types

| Type | Direction | Payload | Reply |
|------|-----------|---------|-------|
| 1 `MSG_ECHO_REQUEST` | frontend -> backend | any text | type 101: `Backend responds to message <text>` |
| 2 `MSG_GET_TIME` | frontend -> backend | empty | type 102: backend wall-clock time |
| 3 `MSG_INCREMENT` | frontend -> backend | empty | type 103: `counter = <n>` |
| 4 `MSG_GET_STATS` | frontend -> backend | empty | type 104: uptime / messages / counter |
| 100 `MSG_WELCOME` | backend -> frontend | greeting | sent when a frontend connects (NEW_COORDINATOR) |
| 101-104 | backend -> frontend | replies | see above |

## QML <-> backend contract

The root QML file must declare (see README.MD of rm-appload):

```qml
signal close
function unloading() { ... }
```

Communication uses the `net.asivery.AppLoad 1.0` module:

```qml
AppLoad {
    id: endpoint
    applicationID: "cpp-backend-example"   // == "id" in manifest.json
    onMessageReceived: (type, contents) => { /* ... */ }
}
// send to backend:  endpoint.sendMessage(type, contents)
// kill backend + all frontends:  endpoint.terminate()   (call it in unloading())
```

Multiple endpoints per app are allowed; all receive all backend messages.

## Notes

- The backend needs **no Qt**, only POSIX sockets — that is the whole point of the
  AppLoad backend protocol (any language can be used; here: plain C++17).
- The launcher icon is stored base64-encoded as `icon.png.b64` (binary files cannot be
  pushed through the GitHub API tools used here). The `build-*.sh` scripts decode it into
  `output*/icon.png`. To use your own icon, either place a real `icon.png` next to the
  build scripts (it is then preferred) or regenerate `icon.png.b64` from it
  (`base64 -w0 icon.png > icon.png.b64`).
