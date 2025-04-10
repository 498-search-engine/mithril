#ifndef QUERY_COORDINATOR_H
#define QUERY_COORDINATOR_H

#include <string>
#include <vector>

namespace mithril {

// dummmy
class QueryCoordinator {
public:
    explicit QueryCoordinator(const std::string& config_path) {}

    std::vector<int> send_query_to_workers(const std::string& query_text) { return {1, 2, 3}; }
};

}  // namespace mithril

#endif  // QUERY_COORDINATOR_H