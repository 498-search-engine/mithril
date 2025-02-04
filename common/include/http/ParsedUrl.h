#ifndef COMMON_HTTP_PARSEDURL_H
#define COMMON_HTTP_PARSEDURL_H

#include <cstring>
#include <string>

namespace mithril::http {

struct ParsedUrl {
    std::string url;
    std::string service;
    std::string host;
    std::string port;
    std::string path;
};

ParsedUrl ParseURL(std::string url);

class ParsedUrl2 {
public:
    const char* CompleteUrl;
    char *Service, *Host, *Port, *Path;

    ParsedUrl2(const char* url) {
        // Assumes url points to static text but
        // does not check.

        CompleteUrl = url;

        pathBuffer = new char[strlen(url) + 1];
        const char* f;
        char* t;
        for (t = pathBuffer, f = url; (*t++ = *f++) != 0;) {
            ;
        }

        Service = pathBuffer;

        const char Colon = ':', Slash = '/';
        char* p;
        for (p = pathBuffer; *p && *p != Colon; p++)
            ;

        if (*p) {
            // Mark the end of the Service.
            *p++ = 0;

            if (*p == Slash)
                p++;
            if (*p == Slash)
                p++;

            Host = p;

            for (; *p && *p != Slash && *p != Colon; p++)
                ;

            if (*p == Colon) {
                // Port specified.  Skip over the colon and
                // the port number.
                *p++ = 0;
                Port = +p;
                for (; *p && *p != Slash; p++)
                    ;
            } else
                Port = p;

            if (*p)
                // Mark the end of the Host and Port.
                *p++ = 0;

            // Whatever remains is the Path.
            Path = p;
        } else
            Host = Path = p;
    }

    ~ParsedUrl2() { delete[] pathBuffer; }

private:
    char* pathBuffer;
};

}  // namespace mithril::http

#endif
