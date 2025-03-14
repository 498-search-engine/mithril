#ifndef COMMON_HTTP_SYNCRESOLVER_H
#define COMMON_HTTP_SYNCRESOLVER_H

#include "http/Resolver.h"

#include <netdb.h>
#include <string>


namespace mithril::http {

class SyncResolver : public Resolver {
public:
    SyncResolver() = default;

    bool Resolve(const std::string& host, const std::string& port, Resolver::ResolutionResult& result) override;

    // TODO cache
};

}  // namespace mithril::http

#endif
