#include "http/SSL.h"

#include <stdexcept>
#include <unistd.h>
#include <openssl/ssl.h>

namespace mithril::http {

namespace internal {
SSL_CTX* SSLCtx = nullptr;
}

void InitializeSSL() {
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();

    auto* ctx = SSL_CTX_new(TLS_client_method());
    if (ctx == nullptr) {
        throw std::runtime_error("Failed to create SSL context");
    }

    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, nullptr);  // TODO: do we care to verify?
    SSL_CTX_set_verify_depth(ctx, 4);
    SSL_CTX_set_default_verify_paths(ctx);

    internal::SSLCtx = ctx;
}

void DeinitializeSSL() {
    if (internal::SSLCtx != nullptr) {
        SSL_CTX_free(internal::SSLCtx);
        internal::SSLCtx = nullptr;
    }
}

}  // namespace mithril::http
