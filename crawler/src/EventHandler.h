#ifndef CRAWLER_EVENTHANDLER_H
#define CRAWLER_EVENTHANDLER_H

#include "http/Connection.h"
#include "http/RequestExecutor.h"

namespace mithril {

class EventHandler {
public:
    void Run();

private:
    void DispatchReadyConnection(http::Connection conn);

    http::RequestExecutor requestExecutor_;
};

}  // namespace mithril

#endif
