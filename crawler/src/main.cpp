#include "Coordinator.h"
#include "http/SSL.h"

#include <cassert>

int main(int argc, char* argv[]) {
    mithril::http::InitializeSSL();

    try {
        auto config = mithril::LoadConfigFromFile(argc > 1 ? argv[1] : "crawler.conf");
        
        mithril::Coordinator c(config);
        c.Run();
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    mithril::http::DeinitializeSSL();

    return 0;
}
