#include "Coordinator.h"
#include "http/SSL.h"

#include <cassert>

int main(int argc, char* argv[]) {
    mithril::http::InitializeSSL();

    mithril::Coordinator c;
    c.Run();

    mithril::http::DeinitializeSSL();

    return 0;
}
