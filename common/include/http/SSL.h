#ifndef COMMON_HTTP_SSL_H
#define COMMON_HTTP_SSL_H

#include <openssl/ssl.h>

namespace mithril::http {

void InitializeSSL();
void DeinitializeSSL();

namespace internal {

extern SSL_CTX* SSLCtx;

}

}  // namespace mithril::http

#endif
