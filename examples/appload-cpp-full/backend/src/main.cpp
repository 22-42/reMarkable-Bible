#include "appload_client.h"
#include "demo_backend.h"

#include <iostream>
#include <memory>

// AppLoad starts this binary with argv[1] set to the path of a temporary
// unix socket (SOCK_SEQPACKET) created by AppLoad. See README.MD of
// https://github.com/asivery/rm-appload ("Applications' format").
int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: " << argv[0] << " <appload unix socket path>" << std::endl;
        return 2;
    }

    appload::AppLoad app(std::make_unique<DemoBackend>(), argv[1]);
    return app.run();
}
