#ifndef WEB_PLUGIN_H
#define WEB_PLUGIN_H

#include "ResponseWriter.h"

#include <string>

namespace mithril {
// Plugin interface for intercepting HTTP requests
class PluginObject {
public:
    // Returns true if this plugin handles the given path
    virtual bool MagicPath(const std::string& path) = 0;
    // Process request and return HTTP response
    virtual void ProcessRequest(std::string request, ResponseWriter& rw) = 0;
    virtual ~PluginObject() {}
};

extern PluginObject* Plugin;
}  // namespace mithril

#endif  // WEB_PLUGIN_H
