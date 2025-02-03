#include "EventHandler.h"

namespace mithril {

void EventHandler::Run() {
    while (true) {
        // TODO: read requests from queue

        // Process ready-to-read connections
        requestExecutor_.ProcessConnections();

        // Process connections with ready responses
        auto& ready = requestExecutor_.ReadyConnections();
        if (!ready.empty()) {
            for (auto& conn : ready) {
                DispatchReadyConnection(std::move(conn));
            }
            ready.clear();
        }

        // TODO: process failed connections
        requestExecutor_.FailedConnections().clear();
    }
}

void EventHandler::DispatchReadyConnection(http::Connection conn) {
    // TODO: pass off to worker thread
}

}  // namespace mithril
